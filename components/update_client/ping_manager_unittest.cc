// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <stdint.h>

#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/component.h"
#include "components/update_client/net/url_loader_post_interceptor.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/test_activity_data_service.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"

namespace update_client {

class PingManagerTest : public testing::Test,
                        public testing::WithParamInterface<bool> {
 public:
  PingManagerTest();
  ~PingManagerTest() override = default;

  PingManager::Callback MakePingCallback();
  scoped_refptr<UpdateContext> MakeMockUpdateContext() const;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void PingSentCallback(int error, const std::string& response);

 protected:
  void Quit();
  void RunThreads();

  scoped_refptr<TestConfigurator> config_;
  scoped_refptr<PingManager> ping_manager_;
  std::unique_ptr<PersistedData> metadata_;

  int error_ = -1;
  std::string response_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::OnceClosure quit_closure_;
  std::unique_ptr<TestActivityDataService> activity_data_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_;
};

PingManagerTest::PingManagerTest()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
  config_ = base::MakeRefCounted<TestConfigurator>();
}

void PingManagerTest::SetUp() {
  ping_manager_ = base::MakeRefCounted<PingManager>(config_);
  pref_ = std::make_unique<TestingPrefServiceSimple>();
  activity_data_service_ = std::make_unique<TestActivityDataService>();
  PersistedData::RegisterPrefs(pref_->registry());
  metadata_ = std::make_unique<PersistedData>(pref_.get(),
                                              activity_data_service_.get());
}

void PingManagerTest::TearDown() {
  // Run the threads until they are idle to allow the clean up
  // of the network interceptors on the IO thread.
  task_environment_.RunUntilIdle();
  ping_manager_ = nullptr;
}

void PingManagerTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();
}

void PingManagerTest::Quit() {
  if (!quit_closure_.is_null())
    std::move(quit_closure_).Run();
}

PingManager::Callback PingManagerTest::MakePingCallback() {
  return base::BindOnce(&PingManagerTest::PingSentCallback,
                        base::Unretained(this));
}

void PingManagerTest::PingSentCallback(int error, const std::string& response) {
  error_ = error;
  response_ = response;
  Quit();
}

scoped_refptr<UpdateContext> PingManagerTest::MakeMockUpdateContext() const {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    return nullptr;
  }
  CrxCache::Options options(temp_dir.GetPath());
  return base::MakeRefCounted<UpdateContext>(
      config_, base::MakeRefCounted<CrxCache>(options), false, false,
      std::vector<std::string>(), UpdateClient::CrxStateChangeCallback(),
      UpdateEngine::NotifyObserversCallback(), UpdateEngine::Callback(),
      nullptr,
      /*is_update_check_only=*/false);
}

// This test is parameterized for using JSON or XML serialization. |true| means
// JSON serialization is used.
INSTANTIATE_TEST_SUITE_P(Parameterized, PingManagerTest, testing::Bool());

TEST_P(PingManagerTest, SendPing) {
  auto interceptor = std::make_unique<URLLoaderPostInterceptor>(
      config_->test_url_loader_factory());
  EXPECT_TRUE(interceptor);

  const auto update_context = MakeMockUpdateContext();
  {
    // Test eventresult="1" is sent for successful updates.
    Component component(*update_context, "abc");
    component.crx_component_ = CrxComponent();
    component.crx_component_->version = base::Version("1.0");
    component.crx_component_->ap = "ap1";
    component.crx_component_->brand = "BRND";
    component.state_ = std::make_unique<Component::StateUpdated>(&component);
    component.previous_version_ = base::Version("1.0");
    component.next_version_ = base::Version("2.0");
    component.AppendEvent(component.MakeEventUpdateComplete());

    metadata_->SetCohort("abc", "c1");
    metadata_->SetCohortName("abc", "cn1");
    metadata_->SetCohortHint("abc", "ch1");

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, *metadata_, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(msg);
    ASSERT_TRUE(root);
    const base::Value* request_val = root->GetDict().Find("request");
    ASSERT_TRUE(request_val);
    const base::Value::Dict& request = request_val->GetDict();

    EXPECT_TRUE(request.contains("@os"));
    EXPECT_EQ("fake_prodid", CHECK_DEREF(request.FindString("@updater")));
    EXPECT_EQ("crx3,puff", CHECK_DEREF(request.FindString("acceptformat")));
    EXPECT_TRUE(request.contains("arch"));
    EXPECT_EQ("cr", CHECK_DEREF(request.FindString("dedup")));
    EXPECT_LT(0, request.FindByDottedPath("hw.physmemory")->GetInt());
    EXPECT_TRUE(request.contains("nacl_arch"));
    EXPECT_EQ("fake_channel_string",
              CHECK_DEREF(request.FindString("prodchannel")));
    EXPECT_EQ("30.0", CHECK_DEREF(request.FindString("prodversion")));
    EXPECT_EQ("3.1", CHECK_DEREF(request.FindString("protocol")));
    EXPECT_TRUE(request.contains("requestid"));
    EXPECT_TRUE(request.contains("sessionid"));
    EXPECT_EQ("fake_channel_string",
              CHECK_DEREF(request.FindString("updaterchannel")));
    EXPECT_EQ("30.0", CHECK_DEREF(request.FindString("updaterversion")));

    EXPECT_TRUE(request.FindByDottedPath("os.arch")->is_string());
    EXPECT_EQ("Fake Operating System",
              request.FindByDottedPath("os.platform")->GetString());
    EXPECT_TRUE(request.FindByDottedPath("os.version")->is_string());

    const base::Value::Dict& app =
        CHECK_DEREF(request.FindList("app"))[0].GetDict();
    EXPECT_EQ("abc", CHECK_DEREF(app.FindString("appid")));
    EXPECT_EQ("ap1", CHECK_DEREF(app.FindString("ap")));
    EXPECT_EQ("BRND", CHECK_DEREF(app.FindString("brand")));
    EXPECT_EQ("fake_lang", CHECK_DEREF(app.FindString("lang")));
    EXPECT_EQ(-1, app.FindInt("installdate"));
    EXPECT_EQ("1.0", CHECK_DEREF(app.FindString("version")));
    EXPECT_EQ("c1", CHECK_DEREF(app.FindString("cohort")));
    EXPECT_EQ("cn1", CHECK_DEREF(app.FindString("cohortname")));
    EXPECT_EQ("ch1", CHECK_DEREF(app.FindString("cohorthint")));
    const base::Value::Dict& event =
        CHECK_DEREF(app.FindList("event"))[0].GetDict();
    EXPECT_EQ(1, event.FindInt("eventresult"));
    EXPECT_EQ(3, event.FindInt("eventtype"));
    EXPECT_EQ("2.0", CHECK_DEREF(event.FindString("nextversion")));
    EXPECT_EQ("1.0", CHECK_DEREF(event.FindString("previousversion")));

    // Check the ping request does not carry the specific extra request headers.
    const auto headers = std::get<1>(interceptor->GetRequests()[0]);
    EXPECT_FALSE(headers.HasHeader("X-Goog-Update-Interactivity"));
    EXPECT_FALSE(headers.HasHeader("X-Goog-Update-Updater"));
    EXPECT_FALSE(headers.HasHeader("X-Goog-Update-AppId"));
    interceptor->Reset();
  }

  {
    // Test eventresult="0" is sent for failed updates.
    Component component(*update_context, "abc");
    component.crx_component_ = CrxComponent();
    component.crx_component_->version = base::Version("1.0");
    component.state_ =
        std::make_unique<Component::StateUpdateError>(&component);
    component.previous_version_ = base::Version("1.0");
    component.next_version_ = base::Version("2.0");
    component.AppendEvent(component.MakeEventUpdateComplete());

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, *metadata_, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
    const absl::optional<base::Value> root_val = base::JSONReader::Read(msg);
    ASSERT_TRUE(root_val);
    const base::Value::Dict& root = root_val->GetDict();
    const base::Value::Dict* request = root.FindDict("request");
    const base::Value& app_val = CHECK_DEREF(request->FindList("app"))[0];
    const base::Value::Dict& app = app_val.GetDict();
    EXPECT_EQ("abc", CHECK_DEREF(app.FindString("appid")));
    EXPECT_EQ("1.0", CHECK_DEREF(app.FindString("version")));
    const base::Value::Dict& event =
        CHECK_DEREF(app.FindList("event"))[0].GetDict();
    EXPECT_EQ(0, event.FindInt("eventresult"));
    EXPECT_EQ(3, event.FindInt("eventtype"));
    EXPECT_EQ("2.0", CHECK_DEREF(event.FindString("nextversion")));
    EXPECT_EQ("1.0", CHECK_DEREF(event.FindString("previousversion")));
    interceptor->Reset();
  }

  {
    // Test the error values and the fingerprints.
    Component component(*update_context, "abc");
    component.crx_component_ = CrxComponent();
    component.crx_component_->version = base::Version("1.0");
    component.state_ =
        std::make_unique<Component::StateUpdateError>(&component);
    component.previous_version_ = base::Version("1.0");
    component.next_version_ = base::Version("2.0");
    component.previous_fp_ = "prev fp";
    component.next_fp_ = "next fp";
    component.error_category_ = ErrorCategory::kDownload;
    component.error_code_ = 2;
    component.extra_code1_ = -1;
    component.diff_error_category_ = ErrorCategory::kService;
    component.diff_error_code_ = 20;
    component.diff_extra_code1_ = -10;
    component.crx_diffurls_.emplace_back("http://host/path");
    component.AppendEvent(component.MakeEventUpdateComplete());

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, *metadata_, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(msg);
    ASSERT_TRUE(root);
    const base::Value::Dict* request = root->GetDict().FindDict("request");
    const base::Value& app_val = CHECK_DEREF(request->FindList("app"))[0];
    const base::Value::Dict& app = app_val.GetDict();
    EXPECT_EQ("abc", CHECK_DEREF(app.FindString("appid")));
    EXPECT_EQ("1.0", CHECK_DEREF(app.FindString("version")));
    const base::Value::Dict& event =
        CHECK_DEREF(app.FindList("event"))[0].GetDict();
    EXPECT_EQ(0, event.FindInt("eventresult"));
    EXPECT_EQ(3, event.FindInt("eventtype"));
    EXPECT_EQ("2.0", CHECK_DEREF(event.FindString("nextversion")));
    EXPECT_EQ("1.0", CHECK_DEREF(event.FindString("previousversion")));
    EXPECT_EQ(4, event.FindInt("differrorcat"));
    EXPECT_EQ(20, event.FindInt("differrorcode"));
    EXPECT_EQ(-10, event.FindInt("diffextracode1"));
    EXPECT_EQ(0, event.FindInt("diffresult"));
    EXPECT_EQ(1, event.FindInt("errorcat"));
    EXPECT_EQ(2, event.FindInt("errorcode"));
    EXPECT_EQ(-1, event.FindInt("extracode1"));
    EXPECT_EQ("next fp", CHECK_DEREF(event.FindString("nextfp")));
    EXPECT_EQ("prev fp", CHECK_DEREF(event.FindString("previousfp")));
    interceptor->Reset();
  }

  {
    // Test an invalid |next_version| is not serialized.
    Component component(*update_context, "abc");
    component.crx_component_ = CrxComponent();
    component.crx_component_->version = base::Version("1.0");
    component.state_ =
        std::make_unique<Component::StateUpdateError>(&component);
    component.previous_version_ = base::Version("1.0");

    component.AppendEvent(component.MakeEventUpdateComplete());

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, *metadata_, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
    const auto root = base::JSONReader::Read(msg);
    ASSERT_TRUE(root);
    const base::Value::Dict* request = root->GetDict().FindDict("request");
    const base::Value::Dict& app =
        CHECK_DEREF(request->FindList("app"))[0].GetDict();
    EXPECT_EQ("abc", CHECK_DEREF(app.FindString("appid")));
    EXPECT_EQ("1.0", CHECK_DEREF(app.FindString("version")));
    const base::Value::Dict& event =
        CHECK_DEREF(app.FindList("event"))[0].GetDict();
    EXPECT_EQ(0, event.FindInt("eventresult"));
    EXPECT_EQ(3, event.FindInt("eventtype"));
    EXPECT_EQ("1.0", CHECK_DEREF(event.FindString("previousversion")));
    interceptor->Reset();
  }

  {
    // Test a valid |previouversion| and |next_version| = base::Version("0")
    // are serialized correctly under <event...> for uninstall.
    Component component(*update_context, "abc");
    CrxComponent crx_component;
    crx_component.version = base::Version("1.2.3.4");
    component.PingOnly(crx_component, 4, 1, 0, 0);

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, *metadata_, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(msg);
      ASSERT_TRUE(root);
      const base::Value::Dict* request = root->GetDict().FindDict("request");
      const base::Value& app_val = CHECK_DEREF(request->FindList("app"))[0];
      const base::Value::Dict& app = app_val.GetDict();
      EXPECT_EQ("abc", CHECK_DEREF(app.FindString("appid")));
      EXPECT_EQ("1.2.3.4", CHECK_DEREF(app.FindString("version")));
      const base::Value::Dict& event =
          CHECK_DEREF(app.FindList("event"))[0].GetDict();
      EXPECT_EQ(1, event.FindInt("eventresult"));
      EXPECT_EQ(4, event.FindInt("eventtype"));
      EXPECT_EQ("1.2.3.4", CHECK_DEREF(event.FindString("previousversion")));
      EXPECT_EQ(event.FindString("nextversion"), nullptr);
      interceptor->Reset();
  }

  {
    // Test the download metrics.
    Component component(*update_context, "abc");
    component.crx_component_ = CrxComponent();
    component.crx_component_->version = base::Version("1.0");
    component.state_ = std::make_unique<Component::StateUpdated>(&component);
    component.previous_version_ = base::Version("1.0");
    component.next_version_ = base::Version("2.0");
    component.AppendEvent(component.MakeEventUpdateComplete());

    CrxDownloader::DownloadMetrics download_metrics;
    download_metrics.url = GURL("http://host1/path1");
    download_metrics.downloader = CrxDownloader::DownloadMetrics::kUrlFetcher;
    download_metrics.error = -1;
    download_metrics.downloaded_bytes = 123;
    download_metrics.total_bytes = 456;
    download_metrics.download_time_ms = 987;
    component.AppendEvent(component.MakeEventDownloadMetrics(download_metrics));

    download_metrics = CrxDownloader::DownloadMetrics();
    download_metrics.url = GURL("http://host2/path2");
    download_metrics.downloader = CrxDownloader::DownloadMetrics::kBits;
    download_metrics.error = 0;
    download_metrics.downloaded_bytes = 1230;
    download_metrics.total_bytes = 4560;
    download_metrics.download_time_ms = 9870;
    component.AppendEvent(component.MakeEventDownloadMetrics(download_metrics));

    download_metrics = CrxDownloader::DownloadMetrics();
    download_metrics.url = GURL("http://host3/path3");
    download_metrics.downloader = CrxDownloader::DownloadMetrics::kBits;
    download_metrics.error = 0;
    download_metrics.downloaded_bytes = protocol_request::kProtocolMaxInt;
    download_metrics.total_bytes = protocol_request::kProtocolMaxInt - 1;
    download_metrics.download_time_ms = protocol_request::kProtocolMaxInt - 2;
    component.AppendEvent(component.MakeEventDownloadMetrics(download_metrics));

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, *metadata_, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(msg);
      ASSERT_TRUE(root);
      const base::Value::Dict* request = root->GetDict().FindDict("request");
      const base::Value& app_val = CHECK_DEREF(request->FindList("app"))[0];
      const base::Value::Dict& app = app_val.GetDict();
      EXPECT_EQ("abc", CHECK_DEREF(app.FindString("appid")));
      EXPECT_EQ("1.0", CHECK_DEREF(app.FindString("version")));
      EXPECT_EQ(4u, CHECK_DEREF(app.FindList("event")).size());
      {
        const base::Value::Dict& event =
            CHECK_DEREF(app.FindList("event"))[0].GetDict();
        EXPECT_EQ(1, event.FindInt("eventresult"));
        EXPECT_EQ(3, event.FindInt("eventtype"));
        EXPECT_EQ("2.0", CHECK_DEREF(event.FindString("nextversion")));
        EXPECT_EQ("1.0", CHECK_DEREF(event.FindString("previousversion")));
      }
      {
        const base::Value::Dict& event =
            CHECK_DEREF(app.FindList("event"))[1].GetDict();
        EXPECT_EQ(0, event.FindInt("eventresult"));
        EXPECT_EQ(14, event.FindInt("eventtype"));
        EXPECT_EQ(987, event.FindDouble("download_time_ms"));
        EXPECT_EQ(123, event.FindDouble("downloaded"));
        EXPECT_EQ("direct", CHECK_DEREF(event.FindString("downloader")));
        EXPECT_EQ(-1, event.FindInt("errorcode"));
        EXPECT_EQ("2.0", CHECK_DEREF(event.FindString("nextversion")));
        EXPECT_EQ("1.0", CHECK_DEREF(event.FindString("previousversion")));
        EXPECT_EQ(456, event.FindDouble("total"));
        EXPECT_EQ("http://host1/path1", CHECK_DEREF(event.FindString("url")));
      }
      {
        const base::Value::Dict& event =
            CHECK_DEREF(app.FindList("event"))[2].GetDict();
        EXPECT_EQ(1, event.FindInt("eventresult"));
        EXPECT_EQ(14, event.FindInt("eventtype"));
        EXPECT_EQ(9870, event.FindDouble("download_time_ms"));
        EXPECT_EQ(1230, event.FindDouble("downloaded"));
        EXPECT_EQ("bits", CHECK_DEREF(event.FindString("downloader")));
        EXPECT_EQ("2.0", CHECK_DEREF(event.FindString("nextversion")));
        EXPECT_EQ("1.0", CHECK_DEREF(event.FindString("previousversion")));
        EXPECT_EQ(4560, event.FindDouble("total"));
        EXPECT_EQ("http://host2/path2", CHECK_DEREF(event.FindString("url")));
      }
      {
        const base::Value::Dict& event =
            CHECK_DEREF(app.FindList("event"))[3].GetDict();
        EXPECT_EQ(1, event.FindInt("eventresult"));
        EXPECT_EQ(14, event.FindInt("eventtype"));
        EXPECT_EQ(9007199254740990, event.FindDouble("download_time_ms"));
        EXPECT_EQ(9007199254740992, event.FindDouble("downloaded"));
        EXPECT_EQ("bits", CHECK_DEREF(event.FindString("downloader")));
        EXPECT_EQ("2.0", CHECK_DEREF(event.FindString("nextversion")));
        EXPECT_EQ("1.0", CHECK_DEREF(event.FindString("previousversion")));
        EXPECT_EQ(9007199254740991, event.FindDouble("total"));
        EXPECT_EQ("http://host3/path3", CHECK_DEREF(event.FindString("url")));
      }
    interceptor->Reset();
  }

  // Tests the presence of the `domain joined` in the ping request.
  {
    for (const auto is_managed : std::initializer_list<absl::optional<bool>>{
             absl::nullopt, false, true}) {
      config_->SetIsMachineExternallyManaged(is_managed);
      EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
      Component component(*update_context, "abc");
      component.crx_component_ = CrxComponent();
      component.previous_version_ = base::Version("1.0");
      component.AppendEvent(component.MakeEventUpdateComplete());
      ping_manager_->SendPing(component, *metadata_, MakePingCallback());

      RunThreads();

      ASSERT_EQ(interceptor->GetCount(), 1);
      const auto root = base::JSONReader::Read(interceptor->GetRequestBody(0));
      interceptor->Reset();

      ASSERT_TRUE(root);
      EXPECT_EQ(is_managed,
                root->GetDict().FindBoolByDottedPath("request.domainjoined"));
    }
  }
  config_->SetIsMachineExternallyManaged(absl::nullopt);
}

// Tests that sending the ping fails when the component requires encryption but
// the ping URL is unsecure.
TEST_P(PingManagerTest, RequiresEncryption) {
  config_->SetPingUrl(GURL("http:\\foo\bar"));

  const auto update_context = MakeMockUpdateContext();

  Component component(*update_context, "abc");
  component.crx_component_ = CrxComponent();
  component.crx_component_->version = base::Version("1.0");

  // The default value for |requires_network_encryption| is true.
  EXPECT_TRUE(component.crx_component_->requires_network_encryption);

  component.state_ = std::make_unique<Component::StateUpdated>(&component);
  component.previous_version_ = base::Version("1.0");
  component.next_version_ = base::Version("2.0");
  component.AppendEvent(component.MakeEventUpdateComplete());

  ping_manager_->SendPing(component, *metadata_, MakePingCallback());
  RunThreads();

  EXPECT_EQ(-2, error_);
}

}  // namespace update_client
