// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "components/update_client/component.h"
#include "components/update_client/net/url_loader_post_interceptor.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/update_engine.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

using std::string;

namespace update_client {

class PingManagerTest : public testing::Test,
                        public testing::WithParamInterface<bool> {
 public:
  PingManagerTest();
  ~PingManagerTest() override {}

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

  int error_ = -1;
  std::string response_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::OnceClosure quit_closure_;
};

PingManagerTest::PingManagerTest()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
  config_ = base::MakeRefCounted<TestConfigurator>();
}

void PingManagerTest::SetUp() {
  ping_manager_ = base::MakeRefCounted<PingManager>(config_);
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
  return base::MakeRefCounted<UpdateContext>(
      config_, false, std::vector<std::string>(),
      UpdateClient::CrxDataCallback(), UpdateEngine::NotifyObserversCallback(),
      UpdateEngine::Callback(), nullptr);
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
    component.state_ = std::make_unique<Component::StateUpdated>(&component);
    component.previous_version_ = base::Version("1.0");
    component.next_version_ = base::Version("2.0");
    component.AppendEvent(component.MakeEventUpdateComplete());

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(msg);
      ASSERT_TRUE(root);
      const auto* request = root->FindKey("request");
      ASSERT_TRUE(request);
      EXPECT_TRUE(request->FindKey("@os"));
      EXPECT_EQ("fake_prodid", request->FindKey("@updater")->GetString());
      EXPECT_EQ("crx2,crx3", request->FindKey("acceptformat")->GetString());
      EXPECT_TRUE(request->FindKey("arch"));
      EXPECT_EQ("cr", request->FindKey("dedup")->GetString());
      EXPECT_LT(0, request->FindPath({"hw", "physmemory"})->GetInt());
      EXPECT_EQ("fake_lang", request->FindKey("lang")->GetString());
      EXPECT_TRUE(request->FindKey("nacl_arch"));
      EXPECT_EQ("fake_channel_string",
                request->FindKey("prodchannel")->GetString());
      EXPECT_EQ("30.0", request->FindKey("prodversion")->GetString());
      EXPECT_EQ("3.1", request->FindKey("protocol")->GetString());
      EXPECT_TRUE(request->FindKey("requestid"));
      EXPECT_TRUE(request->FindKey("sessionid"));
      EXPECT_EQ("fake_channel_string",
                request->FindKey("updaterchannel")->GetString());
      EXPECT_EQ("30.0", request->FindKey("updaterversion")->GetString());

      EXPECT_TRUE(request->FindPath({"os", "arch"})->is_string());
      EXPECT_EQ("Fake Operating System",
                request->FindPath({"os", "platform"})->GetString());
      EXPECT_TRUE(request->FindPath({"os", "version"})->is_string());

      const auto& app = request->FindKey("app")->GetList()[0];
      EXPECT_EQ("abc", app.FindKey("appid")->GetString());
      EXPECT_EQ("1.0", app.FindKey("version")->GetString());
      const auto& event = app.FindKey("event")->GetList()[0];
      EXPECT_EQ(1, event.FindKey("eventresult")->GetInt());
      EXPECT_EQ(3, event.FindKey("eventtype")->GetInt());
      EXPECT_EQ("2.0", event.FindKey("nextversion")->GetString());
      EXPECT_EQ("1.0", event.FindKey("previousversion")->GetString());

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
    ping_manager_->SendPing(component, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(msg);
      ASSERT_TRUE(root);
      const auto* request = root->FindKey("request");
      const auto& app = request->FindKey("app")->GetList()[0];
      EXPECT_EQ("abc", app.FindKey("appid")->GetString());
      EXPECT_EQ("1.0", app.FindKey("version")->GetString());
      const auto& event = app.FindKey("event")->GetList()[0];
      EXPECT_EQ(0, event.FindKey("eventresult")->GetInt());
      EXPECT_EQ(3, event.FindKey("eventtype")->GetInt());
      EXPECT_EQ("2.0", event.FindKey("nextversion")->GetString());
      EXPECT_EQ("1.0", event.FindKey("previousversion")->GetString());
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
    component.crx_diffurls_.push_back(GURL("http://host/path"));
    component.AppendEvent(component.MakeEventUpdateComplete());

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(msg);
      ASSERT_TRUE(root);
      const auto* request = root->FindKey("request");
      const auto& app = request->FindKey("app")->GetList()[0];
      EXPECT_EQ("abc", app.FindKey("appid")->GetString());
      EXPECT_EQ("1.0", app.FindKey("version")->GetString());
      const auto& event = app.FindKey("event")->GetList()[0];
      EXPECT_EQ(0, event.FindKey("eventresult")->GetInt());
      EXPECT_EQ(3, event.FindKey("eventtype")->GetInt());
      EXPECT_EQ("2.0", event.FindKey("nextversion")->GetString());
      EXPECT_EQ("1.0", event.FindKey("previousversion")->GetString());
      EXPECT_EQ(4, event.FindKey("differrorcat")->GetInt());
      EXPECT_EQ(20, event.FindKey("differrorcode")->GetInt());
      EXPECT_EQ(-10, event.FindKey("diffextracode1")->GetInt());
      EXPECT_EQ(0, event.FindKey("diffresult")->GetInt());
      EXPECT_EQ(1, event.FindKey("errorcat")->GetInt());
      EXPECT_EQ(2, event.FindKey("errorcode")->GetInt());
      EXPECT_EQ(-1, event.FindKey("extracode1")->GetInt());
      EXPECT_EQ("next fp", event.FindKey("nextfp")->GetString());
      EXPECT_EQ("prev fp", event.FindKey("previousfp")->GetString());
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
    ping_manager_->SendPing(component, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(msg);
      ASSERT_TRUE(root);
      const auto* request = root->FindKey("request");
      const auto& app = request->FindKey("app")->GetList()[0];
      EXPECT_EQ("abc", app.FindKey("appid")->GetString());
      EXPECT_EQ("1.0", app.FindKey("version")->GetString());
      const auto& event = app.FindKey("event")->GetList()[0];
      EXPECT_EQ(0, event.FindKey("eventresult")->GetInt());
      EXPECT_EQ(3, event.FindKey("eventtype")->GetInt());
      EXPECT_EQ("1.0", event.FindKey("previousversion")->GetString());
    interceptor->Reset();
  }

  {
    // Test a valid |previouversion| and |next_version| = base::Version("0")
    // are serialized correctly under <event...> for uninstall.
    Component component(*update_context, "abc");
    component.crx_component_ = CrxComponent();
    component.Uninstall(base::Version("1.2.3.4"), 0);
    component.AppendEvent(component.MakeEventUninstalled());

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(msg);
      ASSERT_TRUE(root);
      const auto* request = root->FindKey("request");
      const auto& app = request->FindKey("app")->GetList()[0];
      EXPECT_EQ("abc", app.FindKey("appid")->GetString());
      EXPECT_EQ("1.2.3.4", app.FindKey("version")->GetString());
      const auto& event = app.FindKey("event")->GetList()[0];
      EXPECT_EQ(1, event.FindKey("eventresult")->GetInt());
      EXPECT_EQ(4, event.FindKey("eventtype")->GetInt());
      EXPECT_EQ("1.2.3.4", event.FindKey("previousversion")->GetString());
      EXPECT_EQ("0", event.FindKey("nextversion")->GetString());
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
    download_metrics.downloaded_bytes = kProtocolMaxInt;
    download_metrics.total_bytes = kProtocolMaxInt - 1;
    download_metrics.download_time_ms = kProtocolMaxInt - 2;
    component.AppendEvent(component.MakeEventDownloadMetrics(download_metrics));

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component, MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
      const auto root = base::JSONReader::Read(msg);
      ASSERT_TRUE(root);
      const auto* request = root->FindKey("request");
      const auto& app = request->FindKey("app")->GetList()[0];
      EXPECT_EQ("abc", app.FindKey("appid")->GetString());
      EXPECT_EQ("1.0", app.FindKey("version")->GetString());
      EXPECT_EQ(4u, app.FindKey("event")->GetList().size());
      {
        const auto& event = app.FindKey("event")->GetList()[0];
        EXPECT_EQ(1, event.FindKey("eventresult")->GetInt());
        EXPECT_EQ(3, event.FindKey("eventtype")->GetInt());
        EXPECT_EQ("2.0", event.FindKey("nextversion")->GetString());
        EXPECT_EQ("1.0", event.FindKey("previousversion")->GetString());
      }
      {
        const auto& event = app.FindKey("event")->GetList()[1];
        EXPECT_EQ(0, event.FindKey("eventresult")->GetInt());
        EXPECT_EQ(14, event.FindKey("eventtype")->GetInt());
        EXPECT_EQ(987, event.FindKey("download_time_ms")->GetDouble());
        EXPECT_EQ(123, event.FindKey("downloaded")->GetDouble());
        EXPECT_EQ("direct", event.FindKey("downloader")->GetString());
        EXPECT_EQ(-1, event.FindKey("errorcode")->GetInt());
        EXPECT_EQ("2.0", event.FindKey("nextversion")->GetString());
        EXPECT_EQ("1.0", event.FindKey("previousversion")->GetString());
        EXPECT_EQ(456, event.FindKey("total")->GetDouble());
        EXPECT_EQ("http://host1/path1", event.FindKey("url")->GetString());
      }
      {
        const auto& event = app.FindKey("event")->GetList()[2];
        EXPECT_EQ(1, event.FindKey("eventresult")->GetInt());
        EXPECT_EQ(14, event.FindKey("eventtype")->GetInt());
        EXPECT_EQ(9870, event.FindKey("download_time_ms")->GetDouble());
        EXPECT_EQ(1230, event.FindKey("downloaded")->GetDouble());
        EXPECT_EQ("bits", event.FindKey("downloader")->GetString());
        EXPECT_EQ("2.0", event.FindKey("nextversion")->GetString());
        EXPECT_EQ("1.0", event.FindKey("previousversion")->GetString());
        EXPECT_EQ(4560, event.FindKey("total")->GetDouble());
        EXPECT_EQ("http://host2/path2", event.FindKey("url")->GetString());
      }
      {
        const auto& event = app.FindKey("event")->GetList()[3];
        EXPECT_EQ(1, event.FindKey("eventresult")->GetInt());
        EXPECT_EQ(14, event.FindKey("eventtype")->GetInt());
        EXPECT_EQ(9007199254740990,
                  event.FindKey("download_time_ms")->GetDouble());
        EXPECT_EQ(9007199254740992, event.FindKey("downloaded")->GetDouble());
        EXPECT_EQ("bits", event.FindKey("downloader")->GetString());
        EXPECT_EQ("2.0", event.FindKey("nextversion")->GetString());
        EXPECT_EQ("1.0", event.FindKey("previousversion")->GetString());
        EXPECT_EQ(9007199254740991, event.FindKey("total")->GetDouble());
        EXPECT_EQ("http://host3/path3", event.FindKey("url")->GetString());
      }
    interceptor->Reset();
  }
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

  ping_manager_->SendPing(component, MakePingCallback());
  RunThreads();

  EXPECT_EQ(-2, error_);
}

}  // namespace update_client
