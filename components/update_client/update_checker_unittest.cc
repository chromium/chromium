// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_checker.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/component.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/url_loader_post_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;

namespace update_client {

namespace {

base::FilePath test_file(const char* file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("update_client")
      .AppendASCII(file);
}

const char kUpdateItemId[] = "jebgalgnebhfojomionfpkfelancnnkf";

class ActivityDataServiceTest final : public ActivityDataService {
 public:
  bool GetActiveBit(const std::string& id) const override;
  void ClearActiveBit(const std::string& id) override;
  int GetDaysSinceLastActive(const std::string& id) const override;
  int GetDaysSinceLastRollCall(const std::string& id) const override;

  void SetActiveBit(const std::string& id, bool value);
  void SetDaysSinceLastActive(const std::string& id, int daynum);
  void SetDaysSinceLastRollCall(const std::string& id, int daynum);

 private:
  std::map<std::string, bool> actives_;
  std::map<std::string, int> days_since_last_actives_;
  std::map<std::string, int> days_since_last_rollcalls_;
};

bool ActivityDataServiceTest::GetActiveBit(const std::string& id) const {
  const auto& it = actives_.find(id);
  return it != actives_.end() ? it->second : false;
}

void ActivityDataServiceTest::ClearActiveBit(const std::string& id) {
  SetActiveBit(id, false);
}

int ActivityDataServiceTest::GetDaysSinceLastActive(
    const std::string& id) const {
  const auto& it = days_since_last_actives_.find(id);
  return it != days_since_last_actives_.end() ? it->second : -2;
}

int ActivityDataServiceTest::GetDaysSinceLastRollCall(
    const std::string& id) const {
  const auto& it = days_since_last_rollcalls_.find(id);
  return it != days_since_last_rollcalls_.end() ? it->second : -2;
}

void ActivityDataServiceTest::SetActiveBit(const std::string& id, bool value) {
  actives_[id] = value;
}

void ActivityDataServiceTest::SetDaysSinceLastActive(const std::string& id,
                                                     int daynum) {
  days_since_last_actives_[id] = daynum;
}

void ActivityDataServiceTest::SetDaysSinceLastRollCall(const std::string& id,
                                                       int daynum) {
  days_since_last_rollcalls_[id] = daynum;
}

}  // namespace

class UpdateCheckerTest : public testing::Test,
                          public testing::WithParamInterface<bool> {
 public:
  UpdateCheckerTest();
  ~UpdateCheckerTest() override;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void UpdateCheckComplete(
      const base::Optional<ProtocolParser::Results>& results,
      ErrorCategory error_category,
      int error,
      int retry_after_sec);

 protected:
  void Quit();
  void RunThreads();

  std::unique_ptr<Component> MakeComponent() const;

  scoped_refptr<TestConfigurator> config_;
  std::unique_ptr<ActivityDataServiceTest> activity_data_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_;
  std::unique_ptr<PersistedData> metadata_;

  std::unique_ptr<UpdateChecker> update_checker_;

  std::unique_ptr<URLLoaderPostInterceptor> post_interceptor_;

  base::Optional<ProtocolParser::Results> results_;
  ErrorCategory error_category_ = ErrorCategory::kNone;
  int error_ = 0;
  int retry_after_sec_ = 0;

  scoped_refptr<UpdateContext> update_context_;

 private:
  scoped_refptr<UpdateContext> MakeMockUpdateContext() const;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckerTest);
};

INSTANTIATE_TEST_CASE_P(IsForeground, UpdateCheckerTest, testing::Bool());

UpdateCheckerTest::UpdateCheckerTest()
    : scoped_task_environment_(
          base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

UpdateCheckerTest::~UpdateCheckerTest() {
}

void UpdateCheckerTest::SetUp() {
  config_ = base::MakeRefCounted<TestConfigurator>();
  pref_ = std::make_unique<TestingPrefServiceSimple>();
  activity_data_service_ = std::make_unique<ActivityDataServiceTest>();
  PersistedData::RegisterPrefs(pref_->registry());
  metadata_ = std::make_unique<PersistedData>(pref_.get(),
                                              activity_data_service_.get());

  post_interceptor_ = std::make_unique<URLLoaderPostInterceptor>(
      config_->test_url_loader_factory());
  EXPECT_TRUE(post_interceptor_);

  update_checker_ = nullptr;

  error_ = 0;
  retry_after_sec_ = 0;
  update_context_ = MakeMockUpdateContext();
}

void UpdateCheckerTest::TearDown() {
  update_checker_ = nullptr;

  post_interceptor_.reset();

  config_ = nullptr;

  // The PostInterceptor requires the message loop to run to destruct correctly.
  // TODO(sorin): This is fragile and should be fixed.
  scoped_task_environment_.RunUntilIdle();
}

void UpdateCheckerTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();
}

void UpdateCheckerTest::Quit() {
  if (!quit_closure_.is_null())
    std::move(quit_closure_).Run();
}

void UpdateCheckerTest::UpdateCheckComplete(
    const base::Optional<ProtocolParser::Results>& results,
    ErrorCategory error_category,
    int error,
    int retry_after_sec) {
  results_ = results;
  error_category_ = error_category;
  error_ = error;
  retry_after_sec_ = retry_after_sec;
  Quit();
}

scoped_refptr<UpdateContext> UpdateCheckerTest::MakeMockUpdateContext() const {
  return base::MakeRefCounted<UpdateContext>(
      config_, false, std::vector<std::string>(),
      UpdateClient::CrxDataCallback(), UpdateEngine::NotifyObserversCallback(),
      UpdateEngine::Callback(), nullptr);
}

std::unique_ptr<Component> UpdateCheckerTest::MakeComponent() const {
  CrxComponent crx_component;
  crx_component.name = "test_jebg";
  crx_component.pk_hash.assign(jebg_hash, jebg_hash + base::size(jebg_hash));
  crx_component.installer = nullptr;
  crx_component.version = base::Version("0.9");
  crx_component.fingerprint = "fp1";

  auto component = std::make_unique<Component>(*update_context_, kUpdateItemId);
  component->state_ = std::make_unique<Component::StateNew>(component.get());
  component->crx_component_ = crx_component;

  return component;
}

// This test is parameterized for |is_foreground|.
TEST_P(UpdateCheckerTest, UpdateCheckSuccess) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_context_->is_foreground = GetParam();

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];
  component->crx_component_->installer_attributes["ap"] = "some_ap";

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components,
      {{"extra", "params"}, {"testrequest", "1"}}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Sanity check the request.
  const auto& request = post_interceptor_->GetRequestBody(0);
  EXPECT_THAT(request, testing::HasSubstr(
                           R"(request protocol="3.1" dedup="cr" )"
                           R"(acceptformat="crx2,crx3" extra="params" )"
                           R"(testrequest="1")"));
  // The request must not contain any "dlpref" in the default case.
  EXPECT_THAT(request, testing::Not(testing::HasSubstr(R"( dlpref=")")));
  EXPECT_THAT(request,
              testing::HasSubstr(
                  std::string(R"(<app appid=")") + kUpdateItemId +
                  R"(" version="0.9" brand="TEST")" +
                  (GetParam() ? R"( installsource="ondemand")" : "") +
                  R"( ap="some_ap" enabled="1"><updatecheck/><ping r="-2"/>)"));
  EXPECT_THAT(
      request,
      testing::HasSubstr(R"(<packages><package fp="fp1"/></packages></app>)"));
  EXPECT_THAT(request, testing::HasSubstr("<hw physmemory="));

  // Tests that the product id is injected correctly from the configurator.
  EXPECT_THAT(request, testing::HasSubstr(
                           R"( updater="fake_prodid" updaterversion="30.0")"
                           R"( prodversion="30.0" )"));

  // Tests that there is a sessionid attribute.
  EXPECT_THAT(request, testing::HasSubstr(" sessionid="));

  // Sanity check the arguments of the callback after parsing.
  EXPECT_EQ(ErrorCategory::kNone, error_category_);
  EXPECT_EQ(0, error_);
  EXPECT_TRUE(results_);
  EXPECT_EQ(1u, results_->list.size());
  const auto& result = results_->list.front();
  EXPECT_STREQ("jebgalgnebhfojomionfpkfelancnnkf", result.extension_id.c_str());
  EXPECT_EQ("1.0", result.manifest.version);
  EXPECT_EQ("11.0.1.0", result.manifest.browser_min_version);
  EXPECT_EQ(1u, result.manifest.packages.size());
  EXPECT_STREQ("jebgalgnebhfojomionfpkfelancnnkf.crx",
               result.manifest.packages.front().name.c_str());
  EXPECT_EQ(1u, result.crx_urls.size());
  EXPECT_EQ(GURL("http://localhost/download/"), result.crx_urls.front());
  EXPECT_STREQ("this", result.action_run.c_str());

#if (OS_WIN)
  EXPECT_THAT(request, testing::HasSubstr(" domainjoined="));
#if defined(GOOGLE_CHROME_BUILD)
  // Check the Omaha updater state data in the request.
  EXPECT_THAT(request, testing::HasSubstr("<updater "));
  EXPECT_THAT(request, testing::HasSubstr(R"( name="Omaha" )"));
#endif  // GOOGLE_CHROME_BUILD
#endif  // OS_WINDOWS

  // Check the DDOS protection header values.
  const auto extra_request_headers =
      std::get<1>(post_interceptor_->GetRequests()[0]);
  EXPECT_TRUE(extra_request_headers.HasHeader("X-Goog-Update-Interactivity"));
  std::string header;
  extra_request_headers.GetHeader("X-Goog-Update-Interactivity", &header);
  EXPECT_STREQ(GetParam() ? "fg" : "bg", header.c_str());
  extra_request_headers.GetHeader("X-Goog-Update-Updater", &header);
  EXPECT_STREQ("fake_prodid-30.0", header.c_str());
  extra_request_headers.GetHeader("X-Goog-Update-AppId", &header);
  EXPECT_STREQ("jebgalgnebhfojomionfpkfelancnnkf", header.c_str());
}

// Tests that an invalid "ap" is not serialized.
TEST_F(UpdateCheckerTest, UpdateCheckInvalidAp) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  // Make "ap" too long.
  auto& component = components[kUpdateItemId];
  component->crx_component_->installer_attributes["ap"] = std::string(257, 'a');

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));

  RunThreads();

  const auto request = post_interceptor_->GetRequestBody(0);
  EXPECT_THAT(request, testing::HasSubstr(
                           std::string(R"(app appid=")") + kUpdateItemId +
                           R"(" version="0.9" brand="TEST" enabled="1">)" +
                           R"(<updatecheck/><ping r="-2"/>)"));
  EXPECT_THAT(
      request,
      testing::HasSubstr(R"(<packages><package fp="fp1"/></packages></app>)"));
}

TEST_F(UpdateCheckerTest, UpdateCheckSuccessNoBrand) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));

  config_->SetBrand("TOOLONG");   // Sets an invalid brand code.
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));

  RunThreads();

  const auto request = post_interceptor_->GetRequestBody(0);
  EXPECT_THAT(
      request,
      testing::HasSubstr(
          std::string(R"(<app appid=")") + kUpdateItemId +
          R"(" version="0.9" enabled="1"><updatecheck/><ping r="-2"/>)"));
  EXPECT_THAT(
      request,
      testing::HasSubstr(R"(<packages><package fp="fp1"/></packages></app>)"));
}

// Simulates a 403 server response error.
TEST_F(UpdateCheckerTest, UpdateCheckError) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"), net::HTTP_FORBIDDEN));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_EQ(ErrorCategory::kUpdateCheck, error_category_);
  EXPECT_EQ(403, error_);
  EXPECT_FALSE(results_);
}

TEST_F(UpdateCheckerTest, UpdateCheckDownloadPreference) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));

  config_->SetDownloadPreference(string("cacheable"));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components,
      {{"extra", "params"}}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  // The request must contain dlpref="cacheable".
  const auto request = post_interceptor_->GetRequestBody(0);
  EXPECT_THAT(request, testing::HasSubstr(R"( dlpref="cacheable")"));
}

// This test is checking that an update check signed with CUP fails, since there
// is currently no entity that can respond with a valid signed response.
// A proper CUP test requires network mocks, which are not available now.
TEST_F(UpdateCheckerTest, UpdateCheckCupError) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));

  config_->SetEnabledCupSigning(true);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Sanity check the request.
  const auto& request = post_interceptor_->GetRequestBody(0);
  EXPECT_THAT(request, testing::HasSubstr(
                           std::string(R"(<app appid=")") + kUpdateItemId +
                           R"(" version="0.9" brand="TEST" enabled="1">)" +
                           R"(<updatecheck/><ping r="-2"/>)"));
  EXPECT_THAT(
      request,
      testing::HasSubstr(R"(<packages><package fp="fp1"/></packages></app>)"));

  // Expect an error since the response is not trusted.
  EXPECT_EQ(ErrorCategory::kUpdateCheck, error_category_);
  EXPECT_EQ(-10000, error_);
  EXPECT_FALSE(results_);
}

// Tests that the UpdateCheckers will not make an update check for a
// component that requires encryption when the update check URL is unsecure.
TEST_F(UpdateCheckerTest, UpdateCheckRequiresEncryptionError) {
  config_->SetUpdateCheckUrl(GURL("http:\\foo\bar"));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];
  component->crx_component_->requires_network_encryption = true;

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(ErrorCategory::kUpdateCheck, error_category_);
  EXPECT_EQ(-10002, error_);
  EXPECT_FALSE(component->next_version_.IsValid());
}

// Tests that the PersistedData will get correctly update and reserialize
// the elapsed_days value.
TEST_F(UpdateCheckerTest, UpdateCheckLastRollCall) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_4.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_4.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  // Do two update-checks.
  activity_data_service_->SetDaysSinceLastRollCall(kUpdateItemId, 5);
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components,
      {{"extra", "params"}}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components,
      {{"extra", "params"}}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(2, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(2, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_THAT(post_interceptor_->GetRequestBody(0),
              testing::HasSubstr(R"(<ping r="5")"));
  EXPECT_THAT(post_interceptor_->GetRequestBody(1),
              testing::HasSubstr(R"(<ping rd="3383" ping_freshness=)"));
}

TEST_F(UpdateCheckerTest, UpdateCheckLastActive) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_4.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_4.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_4.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  activity_data_service_->SetActiveBit(kUpdateItemId, true);
  activity_data_service_->SetDaysSinceLastActive(kUpdateItemId, 10);
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components,
      {{"extra", "params"}}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  // The active bit should be reset.
  EXPECT_FALSE(metadata_->GetActiveBit(kUpdateItemId));

  activity_data_service_->SetActiveBit(kUpdateItemId, true);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components,
      {{"extra", "params"}}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  // The active bit should be reset.
  EXPECT_FALSE(metadata_->GetActiveBit(kUpdateItemId));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components,
      {{"extra", "params"}}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_FALSE(metadata_->GetActiveBit(kUpdateItemId));

  EXPECT_EQ(3, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(3, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_THAT(post_interceptor_->GetRequestBody(0),
              testing::HasSubstr(R"(<ping a="10" r="-2"/>)"));
  EXPECT_THAT(
      post_interceptor_->GetRequestBody(1),
      testing::HasSubstr(R"(<ping ad="3383" rd="3383" ping_freshness=)"));
  EXPECT_THAT(post_interceptor_->GetRequestBody(2),
              testing::HasSubstr(R"(<ping rd="3383" ping_freshness=)"));
}

TEST_F(UpdateCheckerTest, UpdateCheckInstallSource) {
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];
  auto crx_component = component->crx_component();

  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_THAT(post_interceptor_->GetRequestBody(0),
              testing::Not(testing::HasSubstr("installsource=")));

  update_context_->is_foreground = true;
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& body1 = post_interceptor_->GetRequestBody(1);
  EXPECT_THAT(body1, testing::HasSubstr(R"(installsource="ondemand")"));
  EXPECT_THAT(body1, testing::Not(testing::HasSubstr(R"(installedby=)")));

  update_context_->is_foreground = false;
  crx_component->install_source = "webstore";
  crx_component->install_location = "external";
  component->set_crx_component(*crx_component);
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& body2 = post_interceptor_->GetRequestBody(2);
  EXPECT_THAT(body2, testing::HasSubstr(R"(installsource="webstore")"));
  EXPECT_THAT(body2, testing::HasSubstr(R"(installedby="external")"));

  update_context_->is_foreground = true;
  crx_component->install_source = "sideload";
  crx_component->install_location = "policy";
  component->set_crx_component(*crx_component);
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& body3 = post_interceptor_->GetRequestBody(3);
  EXPECT_THAT(body3, testing::HasSubstr(R"(installsource="sideload")"));
  EXPECT_THAT(body3, testing::HasSubstr(R"(installedby="policy")"));
}

TEST_F(UpdateCheckerTest, ComponentDisabled) {
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];
  auto crx_component = component->crx_component();

  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& body0 = post_interceptor_->GetRequestBody(0);
  EXPECT_THAT(body0, testing::HasSubstr(R"(enabled="1")"));
  EXPECT_THAT(body0, testing::Not(testing::HasSubstr("<disabled")));

  crx_component->disabled_reasons = {};
  component->set_crx_component(*crx_component);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& body1 = post_interceptor_->GetRequestBody(1);
  EXPECT_THAT(body1, testing::HasSubstr(R"(enabled="1")"));
  EXPECT_THAT(body1, testing::Not(testing::HasSubstr("<disabled")));

  crx_component->disabled_reasons = {0};
  component->set_crx_component(*crx_component);
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& body2 = post_interceptor_->GetRequestBody(2);
  EXPECT_THAT(body2, testing::HasSubstr(R"(enabled="0")"));
  EXPECT_THAT(body2, testing::HasSubstr(R"(<disabled reason="0")"));

  crx_component->disabled_reasons = {1};
  component->set_crx_component(*crx_component);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& body3 = post_interceptor_->GetRequestBody(3);
  EXPECT_THAT(body3, testing::HasSubstr(R"(enabled="0")"));
  EXPECT_THAT(body3, testing::HasSubstr(R"(<disabled reason="1")"));

  crx_component->disabled_reasons = {4, 8, 16};
  component->set_crx_component(*crx_component);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& body4 = post_interceptor_->GetRequestBody(4);
  EXPECT_THAT(body4, testing::HasSubstr(R"(enabled="0")"));
  EXPECT_THAT(body4, testing::HasSubstr(R"(<disabled reason="4")"));
  EXPECT_THAT(body4, testing::HasSubstr(R"(<disabled reason="8")"));
  EXPECT_THAT(body4, testing::HasSubstr(R"(<disabled reason="16")"));

  crx_component->disabled_reasons = {0, 4, 8, 16};
  component->set_crx_component(*crx_component);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& body5 = post_interceptor_->GetRequestBody(5);
  EXPECT_THAT(body5, testing::HasSubstr(R"(enabled="0")"));
  EXPECT_THAT(body5, testing::HasSubstr(R"(<disabled reason="0")"));
  EXPECT_THAT(body5, testing::HasSubstr(R"(<disabled reason="4")"));
  EXPECT_THAT(body5, testing::HasSubstr(R"(<disabled reason="8")"));
  EXPECT_THAT(body5, testing::HasSubstr(R"(<disabled reason="16")"));
}

TEST_F(UpdateCheckerTest, UpdateCheckUpdateDisabled) {
  config_->SetBrand("");
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];
  auto crx_component = component->crx_component();

  // Tests the scenario where:
  //  * the component does not support group policies.
  //  * the component updates are disabled.
  // Expects the group policy to be ignored and the update check to not
  // include the "updatedisabled" attribute.
  EXPECT_FALSE(crx_component->supports_group_policy_enable_component_updates);

  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();
  EXPECT_THAT(
      post_interceptor_->GetRequestBody(0),
      testing::HasSubstr(std::string(R"(<app appid=")") + kUpdateItemId +
                         R"(" version="0.9" enabled="1"><updatecheck/>)"));

  // Tests the scenario where:
  //  * the component supports group policies.
  //  * the component updates are disabled.
  // Expects the update check to include the "updatedisabled" attribute.
  crx_component->supports_group_policy_enable_component_updates = true;
  component->set_crx_component(*crx_component);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, false,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();
  EXPECT_THAT(
      post_interceptor_->GetRequestBody(1),
      testing::HasSubstr(std::string(R"(<app appid=")") + kUpdateItemId +
                         R"(" version="0.9" enabled="1">)" +
                         R"(<updatecheck updatedisabled="true"/>)"));

  // Tests the scenario where:
  //  * the component does not support group policies.
  //  * the component updates are enabled.
  // Expects the update check to not include the "updatedisabled" attribute.
  crx_component->supports_group_policy_enable_component_updates = false;
  component->set_crx_component(*crx_component);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();
  EXPECT_THAT(
      post_interceptor_->GetRequestBody(2),
      testing::HasSubstr(std::string(R"(<app appid=")") + kUpdateItemId +
                         R"(" version="0.9" enabled="1"><updatecheck/>)"));

  // Tests the scenario where:
  //  * the component supports group policies.
  //  * the component updates are enabled.
  // Expects the update check to not include the "updatedisabled" attribute.
  crx_component->supports_group_policy_enable_component_updates = true;
  component->set_crx_component(*crx_component);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();
  EXPECT_THAT(
      post_interceptor_->GetRequestBody(3),
      testing::HasSubstr(std::string(R"(<app appid=")") + kUpdateItemId +
                         R"(" version="0.9" enabled="1"><updatecheck/>)"));
}

TEST_F(UpdateCheckerTest, NoUpdateActionRun) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_noupdate.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Sanity check the arguments of the callback after parsing.
  EXPECT_EQ(ErrorCategory::kNone, error_category_);
  EXPECT_EQ(0, error_);
  EXPECT_TRUE(results_);
  EXPECT_EQ(1u, results_->list.size());
  const auto& result = results_->list.front();
  EXPECT_STREQ("jebgalgnebhfojomionfpkfelancnnkf", result.extension_id.c_str());
  EXPECT_STREQ("this", result.action_run.c_str());
}

TEST_F(UpdateCheckerTest, UpdatePauseResume) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  post_interceptor_->url_job_request_ready_callback(base::BindOnce(
      [](URLLoaderPostInterceptor* post_interceptor) {
        post_interceptor->Resume();
      },
      post_interceptor_.get()));
  post_interceptor_->Pause();

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  const auto& request = post_interceptor_->GetRequestBody(0);
  EXPECT_THAT(request, testing::HasSubstr(
                           std::string(R"(<app appid=")") + kUpdateItemId +
                           R"(" version="0.9" brand="TEST" enabled="1">)" +
                           R"(<updatecheck/><ping r="-2"/>)"));
  EXPECT_THAT(
      request,
      testing::HasSubstr(R"(<packages><package fp="fp1"/></packages></app>)"));
}

// Tests that an update checker object and its underlying SimpleURLLoader can
// be safely destroyed while it is paused.
TEST_F(UpdateCheckerTest, UpdateResetUpdateChecker) {
  base::RunLoop runloop;
  auto quit_closure = runloop.QuitClosure();

  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_1.xml")));
  post_interceptor_->url_job_request_ready_callback(base::BindOnce(
      [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
      std::move(quit_closure)));
  post_interceptor_->Pause();

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  runloop.Run();
}

// The update response contains a protocol version which does not match the
// expected protocol version.
TEST_F(UpdateCheckerTest, ParseErrorProtocolVersionMismatch) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_parse_error.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_EQ(ErrorCategory::kUpdateCheck, error_category_);
  EXPECT_EQ(-10003, error_);
  EXPECT_FALSE(results_);
}

// The update response contains a status |error-unknownApplication| for the
// app. The response is succesfully parsed and a result is extracted to
// indicate this status.
TEST_F(UpdateCheckerTest, ParseErrorAppStatusErrorUnknownApplication) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      test_file("updatecheck_reply_unknownapp.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_->session_id, {kUpdateItemId}, components, {}, true,
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_EQ(ErrorCategory::kNone, error_category_);
  EXPECT_EQ(0, error_);
  EXPECT_TRUE(results_);
  EXPECT_EQ(1u, results_->list.size());
  const auto& result = results_->list.front();
  EXPECT_STREQ("error-unknownApplication", result.status.c_str());
}

}  // namespace update_client
