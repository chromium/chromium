// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/update_client/update_checker.h"

#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/component.h"
#include "components/update_client/net/url_loader_post_interceptor.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/test_activity_data_service.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/update_engine.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace update_client {
namespace {

constexpr char kUpdateItemId[] = "jebgalgnebhfojomionfpkfelancnnkf";

}  // namespace

class UpdateCheckerTest : public testing::TestWithParam<bool> {
 public:
  UpdateCheckerTest();

  UpdateCheckerTest(const UpdateCheckerTest&) = delete;
  UpdateCheckerTest& operator=(const UpdateCheckerTest&) = delete;

  ~UpdateCheckerTest() override;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void UpdateCheckComplete(
      const std::optional<ProtocolParser::Results>& results,
      ErrorCategory error_category,
      int error,
      int retry_after_sec);

 protected:
  void Quit();
  void RunThreads();

  std::unique_ptr<Component> MakeComponent() const;
  std::unique_ptr<Component> MakeComponent(const std::string& brand) const;
  std::unique_ptr<Component> MakeComponent(
      const std::string& brand,
      const std::string& install_data_index,
      bool allow_updates_on_metered_connection) const;
  std::optional<base::Value::Dict> ParseRequest(int request_number);
  base::Value GetFirstAppAsValue(const base::Value::Dict& request);
  base::Value::Dict GetFirstAppAsDict(const base::Value::Dict& request);

  std::unique_ptr<TestingPrefServiceSimple> pref_;
  scoped_refptr<TestConfigurator> config_;

  std::unique_ptr<UpdateChecker> update_checker_;

  std::unique_ptr<URLLoaderPostInterceptor> post_interceptor_;

  std::optional<ProtocolParser::Results> results_;
  ErrorCategory error_category_ = ErrorCategory::kNone;
  int error_ = 0;
  int retry_after_sec_ = 0;

  scoped_refptr<UpdateContext> update_context_;

  bool is_foreground_ = false;

 private:
  scoped_refptr<UpdateContext> MakeMockUpdateContext() const;

  base::test::TaskEnvironment task_environment_;
  base::OnceClosure quit_closure_;
  base::ScopedTempDir temp_dir_;
};

// This test is parameterized for |is_foreground|.
INSTANTIATE_TEST_SUITE_P(Parameterized, UpdateCheckerTest, testing::Bool());

UpdateCheckerTest::UpdateCheckerTest()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

UpdateCheckerTest::~UpdateCheckerTest() = default;

void UpdateCheckerTest::SetUp() {
  is_foreground_ = GetParam();

  pref_ = std::make_unique<TestingPrefServiceSimple>();
  RegisterPersistedDataPrefs(pref_->registry());
  config_ = base::MakeRefCounted<TestConfigurator>(pref_.get());

  post_interceptor_ = std::make_unique<URLLoaderPostInterceptor>(
      config_->test_url_loader_factory());
  EXPECT_TRUE(post_interceptor_);

  update_checker_ = nullptr;

  error_ = 0;
  retry_after_sec_ = 0;
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  update_context_ = MakeMockUpdateContext();
  update_context_->is_foreground = is_foreground_;
  update_context_->components_to_check_for_updates = {kUpdateItemId};
}

void UpdateCheckerTest::TearDown() {
  update_checker_ = nullptr;

  post_interceptor_.reset();

  config_ = nullptr;

  // The PostInterceptor requires the message loop to run to destruct correctly.
  task_environment_.RunUntilIdle();
}

void UpdateCheckerTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();
}

void UpdateCheckerTest::Quit() {
  if (!quit_closure_.is_null()) {
    std::move(quit_closure_).Run();
  }
}

void UpdateCheckerTest::UpdateCheckComplete(
    const std::optional<ProtocolParser::Results>& results,
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
  CrxCache::Options options(temp_dir_.GetPath());
  return base::MakeRefCounted<UpdateContext>(
      config_, base::MakeRefCounted<CrxCache>(options), false, false,
      std::vector<std::string>(), UpdateClient::CrxStateChangeCallback(),
      UpdateEngine::Callback(), nullptr,
      /*is_update_check_only=*/false);
}

std::unique_ptr<Component> UpdateCheckerTest::MakeComponent() const {
  return MakeComponent({});
}

std::unique_ptr<Component> UpdateCheckerTest::MakeComponent(
    const std::string& brand) const {
  return MakeComponent(brand, {}, true);
}

std::unique_ptr<Component> UpdateCheckerTest::MakeComponent(
    const std::string& brand,
    const std::string& install_data_index,
    bool allow_updates_on_metered_connection) const {
  CrxComponent crx_component;
  crx_component.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
  crx_component.brand = brand;
  crx_component.install_data_index = install_data_index;
  crx_component.name = "test_jebg";
  crx_component.pk_hash.assign(jebg_hash, jebg_hash + std::size(jebg_hash));
  crx_component.installer = nullptr;
  crx_component.version = base::Version("0.9");
  crx_component.fingerprint = "fp1";
  crx_component.allow_updates_on_metered_connection =
      allow_updates_on_metered_connection;

  auto component = std::make_unique<Component>(*update_context_, kUpdateItemId);
  component->state_ = std::make_unique<Component::StateNew>(component.get());
  component->crx_component_ = crx_component;

  return component;
}

std::optional<base::Value::Dict> UpdateCheckerTest::ParseRequest(
    int request_number) {
  const std::string& request =
      post_interceptor_->GetRequestBody(request_number);
  std::optional<base::Value> request_val = base::JSONReader::Read(request);

  if (!request_val || !request_val->is_dict()) {
    return std::nullopt;
  }

  return std::move(request_val.value()).TakeDict();
}

base::Value UpdateCheckerTest::GetFirstAppAsValue(
    const base::Value::Dict& request) {
  const base::Value::List* app_list =
      request.FindDict("request")->FindList("app");
  return CHECK_DEREF(app_list)[0].Clone();
}

base::Value::Dict UpdateCheckerTest::GetFirstAppAsDict(
    const base::Value::Dict& request) {
  return GetFirstAppAsValue(request).TakeDict();
}

// This test is parameterized for |is_foreground|.
TEST_P(UpdateCheckerTest, UpdateCheckSuccess) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_1.json")));

  config_->SetIsMachineExternallyManaged(true);
  config_->SetUpdaterStateProvider(base::BindRepeating([](bool /*is_machine*/) {
    return UpdaterStateAttributes{{"name", "Omaha"},
                                  {"ismachine", "1"},
                                  {"autoupdatecheckenabled", "1"},
                                  {"updatepolicy", "1"}};
  }));

  config_->GetPersistedData()->SetCohort(kUpdateItemId, "id3");
  config_->GetPersistedData()->SetCohortHint(kUpdateItemId, "hint2");
  config_->GetPersistedData()->SetCohortName(kUpdateItemId, "name1");

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] =
      MakeComponent("TEST", "foobar_install_data_index", true);

  auto& component = update_context_->components[kUpdateItemId];
  component->crx_component_->installer_attributes["ap"] = "some_ap";

  update_checker_->CheckForUpdates(
      update_context_, {{"extra", "params"}, {"testrequest", "1"}},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Check the request.
  std::optional<base::Value::Dict> root = ParseRequest(0);
  ASSERT_TRUE(root);

  const auto* request = root->FindDict("request");
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->contains("@os"));
  ASSERT_TRUE(request->FindString("@updater"));
  EXPECT_EQ("fake_prodid", *request->FindString("@updater"));
  ASSERT_TRUE(request->FindString("acceptformat"));
  EXPECT_EQ("crx3,puff", *request->FindString("acceptformat"));
  EXPECT_TRUE(request->contains("arch"));
  ASSERT_TRUE(request->FindString("dedup"));
  EXPECT_EQ("cr", *request->FindString("dedup"));
  ASSERT_TRUE(request->FindString("extra"));
  EXPECT_EQ("params", *request->FindString("extra"));
  ASSERT_TRUE(request->FindIntByDottedPath("hw.physmemory").has_value());
  EXPECT_LT(0, *request->FindIntByDottedPath("hw.physmemory"));
  EXPECT_TRUE(request->contains("nacl_arch"));
  ASSERT_TRUE(request->FindString("prodchannel"));
  EXPECT_EQ("fake_channel_string", *request->FindString("prodchannel"));
  ASSERT_TRUE(request->FindString("prodversion"));
  EXPECT_EQ("30.0", *request->FindString("prodversion"));
  ASSERT_TRUE(request->FindString("protocol"));
  EXPECT_EQ("3.1", *request->FindString("protocol"));
  EXPECT_TRUE(request->contains("requestid"));
  EXPECT_TRUE(request->contains("sessionid"));
  ASSERT_TRUE(request->FindString("testrequest"));
  EXPECT_EQ("1", *request->FindString("testrequest"));
  ASSERT_TRUE(request->FindString("updaterchannel"));
  EXPECT_EQ("fake_channel_string", *request->FindString("updaterchannel"));
  ASSERT_TRUE(request->FindString("updaterversion"));
  EXPECT_EQ("30.0", *request->FindString("updaterversion"));
  ASSERT_TRUE(request->FindBool("domainjoined").has_value());
  EXPECT_TRUE(request->FindBool("domainjoined").value());

  // No "dlpref" is sent by default.
  EXPECT_FALSE(request->contains("dlpref"));

  EXPECT_TRUE(request->FindStringByDottedPath("os.arch"));
  ASSERT_TRUE(request->FindStringByDottedPath("os.platform"));
  EXPECT_EQ("Fake Operating System",
            *request->FindStringByDottedPath("os.platform"));
  ASSERT_TRUE(request->FindStringByDottedPath("os.version"));

  ASSERT_TRUE(request->FindList("app"));
  ASSERT_FALSE(request->FindList("app")->empty());
  const auto* app = request->FindList("app")->front().GetIfDict();
  ASSERT_TRUE(app);
  ASSERT_TRUE(app->FindString("appid"));
  EXPECT_EQ(kUpdateItemId, *app->FindString("appid"));
  ASSERT_TRUE(app->FindString("version"));
  EXPECT_EQ("0.9", *app->FindString("version"));
  ASSERT_TRUE(app->FindString("brand"));
  EXPECT_EQ("TEST", *app->FindString("brand"));
  ASSERT_TRUE(app->FindString("lang"));
  EXPECT_EQ("fake_lang", *app->FindString("lang"));
  EXPECT_EQ("name1", *app->FindString("cohortname"));
  EXPECT_EQ("hint2", *app->FindString("cohorthint"));
  EXPECT_EQ("id3", *app->FindString("cohort"));

  ASSERT_TRUE(app->FindList("data"));
  ASSERT_FALSE(app->FindList("data")->empty());
  const auto* data = app->FindList("data")->front().GetIfDict();
  ASSERT_TRUE(data);
  ASSERT_TRUE(data->FindString("name"));
  EXPECT_EQ("install", *data->FindString("name"));
  ASSERT_TRUE(data->FindString("index"));
  EXPECT_EQ("foobar_install_data_index", *data->FindString("index"));
  EXPECT_FALSE(data->contains("text"));

  if (is_foreground_) {
    ASSERT_TRUE(app->FindString("installsource"));
    EXPECT_EQ("ondemand", *app->FindString("installsource"));
  }
  ASSERT_TRUE(app->FindString("ap"));
  EXPECT_EQ("some_ap", *app->FindString("ap"));
  ASSERT_TRUE(app->FindBool("enabled").has_value());
  EXPECT_EQ(true, app->FindBool("enabled").value());
  EXPECT_TRUE(app->contains("updatecheck"));
  EXPECT_TRUE(app->contains("ping"));
  ASSERT_TRUE(app->FindIntByDottedPath("ping.r").has_value());
  EXPECT_EQ(-2, app->FindIntByDottedPath("ping.r").value());

  ASSERT_TRUE(app->FindListByDottedPath("packages.package"));
  ASSERT_FALSE(app->FindListByDottedPath("packages.package")->empty());
  ASSERT_TRUE(
      app->FindListByDottedPath("packages.package")->front().GetIfDict());
  ASSERT_TRUE(app->FindListByDottedPath("packages.package")
                  ->front()
                  .GetIfDict()
                  ->FindString("fp"));
  EXPECT_EQ("fp1", *app->FindListByDottedPath("packages.package")
                        ->front()
                        .GetIfDict()
                        ->FindString("fp"));

#if BUILDFLAG(IS_WIN)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const auto* updater = request->FindDict("updater");
  ASSERT_TRUE(updater);
  ASSERT_TRUE(updater->FindString("name"));
  EXPECT_EQ("Omaha", *updater->FindString("name"));
  EXPECT_TRUE(updater->FindBool("autoupdatecheckenabled"));
  EXPECT_TRUE(updater->FindBool("ismachine"));
  EXPECT_TRUE(updater->FindInt("updatepolicy"));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // IS_WIN

  // Check the arguments of the callback after parsing.
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

  // Check the DDOS protection header values.
  const auto extra_request_headers =
      std::get<1>(post_interceptor_->GetRequests()[0]);
  EXPECT_TRUE(extra_request_headers.HasHeader("X-Goog-Update-Interactivity"));
  EXPECT_EQ(extra_request_headers.GetHeader("X-Goog-Update-Interactivity"),
            is_foreground_ ? "fg" : "bg");
  EXPECT_EQ(extra_request_headers.GetHeader("X-Goog-Update-Updater"),
            "fake_prodid-30.0");
  EXPECT_EQ(extra_request_headers.GetHeader("X-Goog-Update-AppId"),
            "jebgalgnebhfojomionfpkfelancnnkf");
}

// Tests that an invalid "ap" is not serialized.
TEST_P(UpdateCheckerTest, UpdateCheckInvalidAp) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_1.json")));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent("TEST");

  // Make "ap" too long.
  auto& component = update_context_->components[kUpdateItemId];
  component->crx_component_->installer_attributes["ap"] = std::string(257, 'a');

  update_checker_->CheckForUpdates(
      update_context_, {},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));

  RunThreads();

  std::optional<base::Value::Dict> root = ParseRequest(0);
  ASSERT_TRUE(root);

  const base::Value app_as_val = GetFirstAppAsValue(root.value());
  const base::Value::Dict app = GetFirstAppAsDict(root.value());

  EXPECT_EQ(kUpdateItemId, CHECK_DEREF(app.FindString("appid")));
  EXPECT_EQ("0.9", CHECK_DEREF(app.FindString("version")));
  EXPECT_EQ("TEST", CHECK_DEREF(app.FindString("brand")));
  EXPECT_FALSE(app.contains("data"));
  if (is_foreground_) {
    EXPECT_EQ("ondemand", CHECK_DEREF(app.FindString("installsource")));
  }
  EXPECT_FALSE(app.contains("ap"));
  EXPECT_EQ(true, app.FindBool("enabled"));
  EXPECT_TRUE(app.contains("updatecheck"));
  EXPECT_TRUE(app.contains("ping"));
  EXPECT_EQ(-2, app_as_val.GetDict().FindByDottedPath("ping.r")->GetInt());
  EXPECT_EQ("fp1", CHECK_DEREF(app_as_val.GetDict()
                                   .FindByDottedPath("packages.package")
                                   ->GetList()[0]
                                   .GetDict()
                                   .FindString("fp")));
}

TEST_P(UpdateCheckerTest, UpdateCheckSuccessNoBrand) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_1.json")));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent("TOOLONG");

  update_checker_->CheckForUpdates(
      update_context_, {},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));

  RunThreads();

  std::optional<base::Value::Dict> root = ParseRequest(0);
  ASSERT_TRUE(root);

  const base::Value app_as_val = GetFirstAppAsValue(root.value());
  const base::Value::Dict app = GetFirstAppAsDict(root.value());
  EXPECT_EQ(kUpdateItemId, CHECK_DEREF(app.FindString("appid")));
  EXPECT_EQ("0.9", CHECK_DEREF(app.FindString("version")));
  EXPECT_FALSE(app.contains("brand"));
  if (is_foreground_) {
    EXPECT_EQ("ondemand", CHECK_DEREF(app.FindString("installsource")));
  }
  EXPECT_EQ(true, app.FindBool("enabled"));
  EXPECT_TRUE(app.contains("updatecheck"));
  EXPECT_TRUE(app.contains("ping"));
  EXPECT_EQ(-2, app_as_val.GetDict().FindByDottedPath("ping.r")->GetInt());
  EXPECT_EQ("fp1", CHECK_DEREF(app_as_val.GetDict()
                                   .FindByDottedPath("packages.package")
                                   ->GetList()[0]
                                   .GetDict()
                                   .FindString("fp")));
}

TEST_P(UpdateCheckerTest, UpdateCheckFallback) {
  config_->SetUpdateCheckUrls(
      {GURL("https://localhost2/update2"), GURL("https://localhost2/update2")});

  // 404 first.
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"), net::HTTP_NOT_FOUND));
  // Then OK.
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_1.json")));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent("TOOLONG");

  update_checker_->CheckForUpdates(
      update_context_, {},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));

  RunThreads();
  EXPECT_EQ(2, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(2, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
}

// Simulates a 403 server response error.
TEST_P(UpdateCheckerTest, UpdateCheckError) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"), net::HTTP_FORBIDDEN));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_, {},
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

TEST_P(UpdateCheckerTest, UpdateCheckDownloadPreference) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_1.json")));

  config_->SetDownloadPreference(std::string("cacheable"));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_, {{"extra", "params"}},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  // The request must contain dlpref="cacheable".
  std::optional<base::Value::Dict> root = ParseRequest(0);
  ASSERT_TRUE(root);
  EXPECT_EQ("cacheable",
            CHECK_DEREF(root->FindDict("request")->FindString("dlpref")));
}

// This test is checking that an update check signed with CUP fails, since there
// is currently no entity that can respond with a valid signed response.
// A proper CUP test requires network mocks, which are not available now.
TEST_P(UpdateCheckerTest, UpdateCheckCupError) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_1.json")));

  config_->SetEnabledCupSigning(true);
  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent("TEST");

  update_checker_->CheckForUpdates(
      update_context_, {},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Check the request.
  std::optional<base::Value::Dict> root = ParseRequest(0);
  ASSERT_TRUE(root);
  const base::Value app_as_val = GetFirstAppAsValue(root.value());
  const base::Value::Dict app = GetFirstAppAsDict(root.value());
  EXPECT_EQ(kUpdateItemId, CHECK_DEREF(app.FindString("appid")));
  EXPECT_EQ("0.9", CHECK_DEREF(app.FindString("version")));
  EXPECT_EQ("TEST", CHECK_DEREF(app.FindString("brand")));
  if (is_foreground_) {
    EXPECT_EQ("ondemand", CHECK_DEREF(app.FindString("installsource")));
  }
  EXPECT_EQ(true, app.FindBool("enabled"));
  EXPECT_TRUE(app.contains("updatecheck"));
  EXPECT_TRUE(app.contains("ping"));
  EXPECT_EQ(-2, app_as_val.GetDict().FindByDottedPath("ping.r")->GetInt());
  EXPECT_EQ("fp1", CHECK_DEREF(app_as_val.GetDict()
                                   .FindByDottedPath("packages.package")
                                   ->GetList()[0]
                                   .GetDict()
                                   .FindString("fp")));

  // Expect an error since the response is not trusted.
  EXPECT_EQ(ErrorCategory::kUpdateCheck, error_category_);
  EXPECT_EQ(-10000, error_);
  EXPECT_FALSE(results_);
}

// Tests that the UpdateCheckers will not make an update check for a
// component that requires encryption when the update check URL is unsecure.
TEST_P(UpdateCheckerTest, UpdateCheckRequiresEncryptionError) {
  config_->SetUpdateCheckUrl(GURL("http:\\foo\bar"));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  auto& component = update_context_->components[kUpdateItemId];
  component->crx_component_->requires_network_encryption = true;

  update_checker_->CheckForUpdates(
      update_context_, {},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(ErrorCategory::kUpdateCheck, error_category_);
  EXPECT_EQ(-10002, error_);
  EXPECT_FALSE(component->next_version_.IsValid());
}

// Tests that the PersistedData will get correctly update and reserialize
// the elapsed_days value.
TEST_P(UpdateCheckerTest, UpdateCheckLastRollCall) {
  const char* filename = "updatecheck_reply_4.json";
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath(filename)));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath(filename)));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  // Do two update-checks.
  config_->GetActivityDataService()->SetDaysSinceLastRollCall(kUpdateItemId, 5);
  update_checker_->CheckForUpdates(
      update_context_, {{"extra", "params"}},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  update_checker_ = UpdateChecker::Create(config_);
  update_checker_->CheckForUpdates(
      update_context_, {{"extra", "params"}},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(2, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(2, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  std::optional<base::Value::Dict> root1 = ParseRequest(0);
  ASSERT_TRUE(root1);
  const base::Value app1 = GetFirstAppAsValue(root1.value());
  EXPECT_EQ(5, app1.GetDict().FindByDottedPath("ping.r")->GetInt());
  std::optional<base::Value::Dict> root2 = ParseRequest(1);
  ASSERT_TRUE(root2);
  const base::Value app2 = GetFirstAppAsValue(root2.value());
  EXPECT_EQ(3383, app2.GetDict().FindByDottedPath("ping.rd")->GetInt());
  EXPECT_TRUE(
      app2.GetDict().FindByDottedPath("ping.ping_freshness")->is_string());
}

TEST_P(UpdateCheckerTest, UpdateCheckLastActive) {
  const char* filename = "updatecheck_reply_4.json";
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath(filename)));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath(filename)));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath(filename)));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  config_->GetActivityDataService()->SetActiveBit(kUpdateItemId, true);
  config_->GetActivityDataService()->SetDaysSinceLastActive(kUpdateItemId, 10);
  update_checker_->CheckForUpdates(
      update_context_, {{"extra", "params"}},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  // The active bit should be reset.
  EXPECT_FALSE(config_->GetActivityDataService()->GetActiveBit(kUpdateItemId));

  config_->GetActivityDataService()->SetActiveBit(kUpdateItemId, true);
  update_checker_ = UpdateChecker::Create(config_);
  update_checker_->CheckForUpdates(
      update_context_, {{"extra", "params"}},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  // The active bit should be reset.
  EXPECT_FALSE(config_->GetActivityDataService()->GetActiveBit(kUpdateItemId));

  update_checker_ = UpdateChecker::Create(config_);
  update_checker_->CheckForUpdates(
      update_context_, {{"extra", "params"}},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_FALSE(config_->GetActivityDataService()->GetActiveBit(kUpdateItemId));

  EXPECT_EQ(3, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(3, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  {
    std::optional<base::Value::Dict> root = ParseRequest(0);
    ASSERT_TRUE(root);
    const base::Value app = GetFirstAppAsValue(root.value());
    EXPECT_EQ(10, app.GetDict().FindIntByDottedPath("ping.a").value());
    EXPECT_EQ(-2, app.GetDict().FindIntByDottedPath("ping.r").value());
  }
  {
    std::optional<base::Value::Dict> root = ParseRequest(1);
    ASSERT_TRUE(root);
    const base::Value app = GetFirstAppAsValue(root.value());
    EXPECT_EQ(3383, app.GetDict().FindByDottedPath("ping.ad")->GetInt());
    EXPECT_EQ(3383, app.GetDict().FindByDottedPath("ping.rd")->GetInt());
    EXPECT_TRUE(
        app.GetDict().FindByDottedPath("ping.ping_freshness")->is_string());
  }
  {
    std::optional<base::Value::Dict> root = ParseRequest(2);
    ASSERT_TRUE(root);
    const base::Value app = GetFirstAppAsValue(root.value());
    EXPECT_EQ(3383, app.GetDict().FindByDottedPath("ping.rd")->GetInt());
    EXPECT_TRUE(
        app.GetDict().FindByDottedPath("ping.ping_freshness")->is_string());
  }
}

TEST_P(UpdateCheckerTest, UpdateCheckInstallSource) {
  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  auto& component = update_context_->components[kUpdateItemId];
  auto crx_component = component->crx_component();

  if (is_foreground_) {
    {
      auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
          config_->test_url_loader_factory());
      EXPECT_TRUE(post_interceptor->ExpectRequest(
          std::make_unique<PartialMatch>("updatecheck"),
          GetTestFilePath("updatecheck_reply_1.json")));
      update_checker_->CheckForUpdates(
          update_context_, {},
          base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                         base::Unretained(this)));
      RunThreads();
      const auto& request = post_interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(request);
      ASSERT_TRUE(root);
      const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
      EXPECT_EQ("ondemand", CHECK_DEREF(app.FindString("installsource")));
      EXPECT_FALSE(app.contains("installedby"));
    }
    {
      auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
          config_->test_url_loader_factory());
      EXPECT_TRUE(post_interceptor->ExpectRequest(
          std::make_unique<PartialMatch>("updatecheck"),
          GetTestFilePath("updatecheck_reply_1.json")));
      crx_component->install_source = "sideload";
      crx_component->install_location = "policy";
      component->set_crx_component(*crx_component);
      update_checker_->CheckForUpdates(
          update_context_, {},
          base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                         base::Unretained(this)));
      RunThreads();
      const auto& request = post_interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(request);
      ASSERT_TRUE(root);
      const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
      EXPECT_EQ("sideload", CHECK_DEREF(app.FindString("installsource")));
      EXPECT_EQ("policy", CHECK_DEREF(app.FindString("installedby")));
    }
    return;
  }

  CHECK(!is_foreground_);
  {
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_FALSE(app.contains("installsource"));
  }
  {
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    crx_component->install_source = "webstore";
    crx_component->install_location = "external";
    component->set_crx_component(*crx_component);
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ("webstore", CHECK_DEREF(app.FindString("installsource")));
    EXPECT_EQ("external", CHECK_DEREF(app.FindString("installedby")));
  }
}

TEST_P(UpdateCheckerTest, ComponentDisabled) {
  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  auto& component = update_context_->components[kUpdateItemId];
  auto crx_component = component->crx_component();

  {
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(true, app.FindBool("enabled"));
    EXPECT_FALSE(app.contains("disabled"));
  }

  {
    crx_component->disabled_reasons = {};
    component->set_crx_component(*crx_component);
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(true, app.FindBool("enabled"));
    EXPECT_FALSE(app.contains("disabled"));
  }

  {
    crx_component->disabled_reasons = {0};
    component->set_crx_component(*crx_component);
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(false, app.FindBool("enabled"));
    const base::Value::List* disabled = app.FindList("disabled");
    EXPECT_EQ(1u, disabled->size());
    EXPECT_EQ(0, CHECK_DEREF(disabled)[0].GetDict().FindInt("reason"));
  }
  {
    crx_component->disabled_reasons = {1};
    component->set_crx_component(*crx_component);
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(false, app.FindBool("enabled"));
    const base::Value::List* disabled = app.FindList("disabled");
    EXPECT_EQ(1u, disabled->size());
    EXPECT_EQ(1, CHECK_DEREF(disabled)[0].GetDict().FindInt("reason"));
  }

  {
    crx_component->disabled_reasons = {4, 8, 16};
    component->set_crx_component(*crx_component);
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(false, app.FindBool("enabled"));
    const base::Value::List& disabled = CHECK_DEREF(app.FindList("disabled"));
    EXPECT_EQ(3u, disabled.size());
    EXPECT_EQ(4, disabled[0].GetDict().FindInt("reason"));
    EXPECT_EQ(8, disabled[1].GetDict().FindInt("reason"));
    EXPECT_EQ(16, disabled[2].GetDict().FindInt("reason"));
  }

  {
    crx_component->disabled_reasons = {0, 4, 8, 16};
    component->set_crx_component(*crx_component);
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(false, app.FindBool("enabled"));
    const base::Value::List& disabled = CHECK_DEREF(app.FindList("disabled"));
    EXPECT_EQ(4u, disabled.size());
    EXPECT_EQ(0, disabled[0].GetDict().FindInt("reason"));
    EXPECT_EQ(4, disabled[1].GetDict().FindInt("reason"));
    EXPECT_EQ(8, disabled[2].GetDict().FindInt("reason"));
    EXPECT_EQ(16, disabled[3].GetDict().FindInt("reason"));
  }
}

TEST_P(UpdateCheckerTest, UpdateCheckUpdateDisabled) {
  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  auto& component = update_context_->components[kUpdateItemId];
  auto crx_component = component->crx_component();

  // Ignore this test parameter to keep the test simple.
  update_context_->is_foreground = false;
  {
    // Tests the scenario where:
    //  * the component updates are enabled.
    // Expects the group policy to be ignored and the update check to not
    // include the "updatedisabled" attribute.
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(kUpdateItemId, CHECK_DEREF(app.FindString("appid")));
    EXPECT_EQ("0.9", CHECK_DEREF(app.FindString("version")));
    EXPECT_EQ(true, app.FindBool("enabled"));
    EXPECT_TRUE(app.FindDict("updatecheck")->empty());
  }
  {
    // Tests the scenario where:
    //  * the component updates are disabled.
    // Expects the update check to include the "updatedisabled" attribute.
    crx_component->updates_enabled = false;
    component->set_crx_component(*crx_component);
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value app_as_val = GetFirstAppAsValue(root->GetDict());
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(kUpdateItemId, CHECK_DEREF(app.FindString("appid")));
    EXPECT_EQ("0.9", CHECK_DEREF(app.FindString("version")));
    EXPECT_EQ(true, app.FindBool("enabled"));
    EXPECT_TRUE(app_as_val.GetDict()
                    .FindBoolByDottedPath("updatecheck.updatedisabled")
                    .value());
  }
}

TEST_P(UpdateCheckerTest, UpdateDisabledByMeteredConnection) {
  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] =
      MakeComponent("TEST", "foobar_install_data_index", false);

  auto& component = update_context_->components[kUpdateItemId];
  auto crx_component = component->crx_component();

  // Ignore this test parameter to keep the test simple.
  update_context_->is_foreground = false;
  {
    // Tests the scenario where:
    //  * the component updates are enabled on a non-metered connection.
    // Expects the the update check to not include the "updatedisabled"
    // attribute.
    config_->SetIsNetworkConnectionMetered(false);
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(kUpdateItemId, CHECK_DEREF(app.FindString("appid")));
    EXPECT_EQ("0.9", CHECK_DEREF(app.FindString("version")));
    EXPECT_EQ(true, app.FindBool("enabled"));
    EXPECT_TRUE(app.FindDict("updatecheck")->empty());
  }
  {
    // Tests the scenario where:
    //  * updates are disabled due to a metered network connection.
    // Expects the update check to include the "updatedisabled" attribute.
    config_->SetIsNetworkConnectionMetered(true);
    component->set_crx_component(*crx_component);
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_1.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const base::Value app_as_val = GetFirstAppAsValue(root->GetDict());
    const base::Value::Dict app = GetFirstAppAsDict(root->GetDict());
    EXPECT_EQ(kUpdateItemId, CHECK_DEREF(app.FindString("appid")));
    EXPECT_EQ("0.9", CHECK_DEREF(app.FindString("version")));
    EXPECT_EQ(true, app.FindBool("enabled"));
    EXPECT_TRUE(app_as_val.GetDict()
                    .FindBoolByDottedPath("updatecheck.updatedisabled")
                    .value());
  }
}

TEST_P(UpdateCheckerTest, SameVersionUpdateAllowed) {
  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  auto& component = update_context_->components[kUpdateItemId];
  auto crx_component = component->crx_component();
  EXPECT_FALSE(crx_component->same_version_update_allowed);
  {
    // Tests that `same_version_update_allowed` is not serialized when its
    // value is false.
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_noupdate.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const auto& app =
        (*root->GetDict().FindListByDottedPath("request.app"))[0].GetDict();
    EXPECT_STREQ(kUpdateItemId, app.FindString("appid")->c_str());
    EXPECT_TRUE(app.FindDict("updatecheck"));
    EXPECT_FALSE(app.FindByDottedPath("updatecheck.sameversionupdate"));
  }
  {
    // Tests that `same_version_update_allowed` is serialized when its
    // value is true.
    crx_component->same_version_update_allowed = true;
    component->set_crx_component(*crx_component);
    auto post_interceptor = std::make_unique<URLLoaderPostInterceptor>(
        config_->test_url_loader_factory());
    EXPECT_TRUE(post_interceptor->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_noupdate.json")));
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();
    const auto& request = post_interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const auto& app =
        (*root->GetDict().FindListByDottedPath("request.app"))[0].GetDict();
    EXPECT_STREQ(kUpdateItemId, app.FindString("appid")->c_str());
    EXPECT_EQ(app.FindBoolByDottedPath("updatecheck.sameversionupdate"), true);
  }
}

TEST_P(UpdateCheckerTest, NoUpdateActionRun) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_noupdate.json")));
  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_, {},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Check the arguments of the callback after parsing.
  EXPECT_EQ(ErrorCategory::kNone, error_category_);
  EXPECT_EQ(0, error_);
  EXPECT_TRUE(results_);
  EXPECT_EQ(1u, results_->list.size());
  const auto& result = results_->list.front();
  EXPECT_STREQ("jebgalgnebhfojomionfpkfelancnnkf", result.extension_id.c_str());
  EXPECT_STREQ("noupdate", result.status.c_str());
  EXPECT_STREQ("this", result.action_run.c_str());
}

TEST_P(UpdateCheckerTest, UpdatePauseResume) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_noupdate.json")));
  post_interceptor_->url_job_request_ready_callback(base::BindOnce(
      [](URLLoaderPostInterceptor* post_interceptor) {
        post_interceptor->Resume();
      },
      post_interceptor_.get()));
  post_interceptor_->Pause();

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent("TEST");

  // Ignore this test parameter to keep the test simple.
  update_context_->is_foreground = false;

  update_checker_->CheckForUpdates(
      update_context_, {},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  RunThreads();

  std::optional<base::Value::Dict> root = ParseRequest(0);
  ASSERT_TRUE(root);
  const base::Value app_as_val = GetFirstAppAsValue(root.value());
  const base::Value::Dict app = GetFirstAppAsDict(root.value());
  EXPECT_EQ(kUpdateItemId, CHECK_DEREF(app.FindString("appid")));
  EXPECT_EQ("0.9", CHECK_DEREF(app.FindString("version")));
  EXPECT_EQ("TEST", CHECK_DEREF(app.FindString("brand")));
  EXPECT_EQ(true, app.FindBool("enabled"));
  EXPECT_TRUE(app.FindDict("updatecheck")->empty());
  EXPECT_EQ(-2, app_as_val.GetDict().FindIntByDottedPath("ping.r").value());

  const base::Value::List& packages =
      CHECK_DEREF(app.FindDict("packages")->FindList("package"));
  EXPECT_EQ("fp1", CHECK_DEREF(packages[0].GetDict().FindString("fp")));
}

// Tests that an update checker object and its underlying SimpleURLLoader can
// be safely destroyed while it is paused.
TEST_P(UpdateCheckerTest, UpdateResetUpdateChecker) {
  base::RunLoop runloop;
  auto quit_closure = runloop.QuitClosure();

  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_1.json")));
  post_interceptor_->url_job_request_ready_callback(base::BindOnce(
      [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
      std::move(quit_closure)));
  post_interceptor_->Pause();

  update_context_->components[kUpdateItemId] = MakeComponent();

  update_checker_ = UpdateChecker::Create(config_);
  update_checker_->CheckForUpdates(
      update_context_, {},
      base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                     base::Unretained(this)));
  runloop.Run();
}

// The update response contains a protocol version which does not match the
// expected protocol version.
TEST_P(UpdateCheckerTest, ParseErrorProtocolVersionMismatch) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_parse_error.json")));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_, {},
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
// app. The response is successfully parsed and a result is extracted to
// indicate this status.
TEST_P(UpdateCheckerTest, ParseErrorAppStatusErrorUnknownApplication) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<PartialMatch>("updatecheck"),
      GetTestFilePath("updatecheck_reply_unknownapp.json")));

  update_checker_ = UpdateChecker::Create(config_);

  update_context_->components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      update_context_, {},
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

TEST_P(UpdateCheckerTest, DomainJoined) {
  for (const auto& is_managed :
       std::initializer_list<std::optional<bool>>{std::nullopt, false, true}) {
    EXPECT_TRUE(post_interceptor_->ExpectRequest(
        std::make_unique<PartialMatch>("updatecheck"),
        GetTestFilePath("updatecheck_reply_noupdate.json")));
    update_checker_ = UpdateChecker::Create(config_);

    update_context_->components[kUpdateItemId] = MakeComponent();

    config_->SetIsMachineExternallyManaged(is_managed);
    update_checker_->CheckForUpdates(
        update_context_, {},
        base::BindOnce(&UpdateCheckerTest::UpdateCheckComplete,
                       base::Unretained(this)));
    RunThreads();

    ASSERT_EQ(post_interceptor_->GetCount(), 1);
    std::optional<base::Value::Dict> root = ParseRequest(0);
    ASSERT_TRUE(root);
    post_interceptor_->Reset();

    // What is injected in the update checker by the configurator must
    // match what is sent in the update check.
    EXPECT_EQ(is_managed, root->FindBoolByDottedPath("request.domainjoined"));
  }
}

}  // namespace update_client
