// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <stdint.h>

#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/component.h"
#include "components/update_client/net/url_loader_post_interceptor.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace update_client {

class PingManagerTest : public testing::Test,
                        public testing::WithParamInterface<bool> {
 public:
  PingManagerTest();
  ~PingManagerTest() override = default;

  base::OnceClosure MakePingCallback();
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

 private:
  base::test::TaskEnvironment task_environment_;
  base::OnceClosure quit_closure_;
  std::unique_ptr<TestingPrefServiceSimple> pref_;
};

PingManagerTest::PingManagerTest()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
  pref_ = std::make_unique<TestingPrefServiceSimple>();
  RegisterPersistedDataPrefs(pref_->registry());
}

void PingManagerTest::SetUp() {
  config_ = base::MakeRefCounted<TestConfigurator>(pref_.get());
  ping_manager_ = base::MakeRefCounted<PingManager>(config_);
}

void PingManagerTest::TearDown() {
  // Run the threads until they are idle to allow the clean up
  // of the network interceptors on the IO thread.
  task_environment_.RunUntilIdle();
  ping_manager_ = nullptr;
  config_ = nullptr;
}

void PingManagerTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();
}

void PingManagerTest::Quit() {
  if (!quit_closure_.is_null()) {
    std::move(quit_closure_).Run();
  }
}

base::OnceClosure PingManagerTest::MakePingCallback() {
  return base::BindOnce(&PingManagerTest::Quit, base::Unretained(this));
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
      UpdateEngine::Callback(), nullptr,
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
    component.crx_component_->app_id = "abc";
    component.crx_component_->version = base::Version("1.0");
    component.crx_component_->ap = "ap1";
    component.crx_component_->brand = "BRND";
    component.state_ = std::make_unique<Component::StateUpdated>(&component);
    component.previous_version_ = base::Version("1.0");
    component.next_version_ = base::Version("2.0");
    component.AppendEvent(component.MakeEventUpdateComplete());

    config_->GetPersistedData()->SetCohort("abc", "c1");
    config_->GetPersistedData()->SetCohortName("abc", "cn1");
    config_->GetPersistedData()->SetCohortHint("abc", "ch1");

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component.session_id(), *component.crx_component_,
                            component.GetEvents(), MakePingCallback());
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
    component.crx_component_->app_id = "abc";
    component.crx_component_->version = base::Version("1.0");
    component.state_ =
        std::make_unique<Component::StateUpdateError>(&component);
    component.previous_version_ = base::Version("1.0");
    component.next_version_ = base::Version("2.0");
    component.AppendEvent(component.MakeEventUpdateComplete());

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component.session_id(), *component.crx_component_,
                            component.GetEvents(), MakePingCallback());
    RunThreads();

    EXPECT_EQ(1, interceptor->GetCount()) << interceptor->GetRequestsAsString();
    const auto msg = interceptor->GetRequestBody(0);
    const std::optional<base::Value> root_val = base::JSONReader::Read(msg);
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
    component.crx_component_->app_id = "abc";
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
    ping_manager_->SendPing(component.session_id(), *component.crx_component_,
                            component.GetEvents(), MakePingCallback());
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
    component.crx_component_->app_id = "abc";
    component.crx_component_->version = base::Version("1.0");
    component.state_ =
        std::make_unique<Component::StateUpdateError>(&component);
    component.previous_version_ = base::Version("1.0");

    component.AppendEvent(component.MakeEventUpdateComplete());

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component.session_id(), *component.crx_component_,
                            component.GetEvents(), MakePingCallback());
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
    CrxComponent crx_component;
    crx_component.app_id = "abc";
    crx_component.version = base::Version("1.2.3.4");

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    base::MakeRefCounted<UpdateEngine>(
        config_,
        base::BindRepeating(
            [](scoped_refptr<Configurator>) -> std::unique_ptr<UpdateChecker> {
              return nullptr;
            }),
        ping_manager_, base::DoNothing())
        ->SendPing(crx_component, {.event_type = 4, .result = 1},
                   base::BindLambdaForTesting([&](Error) { Quit(); }));
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

  // Tests the presence of the `domain joined` in the ping request.
  {
    for (const auto& is_managed : std::initializer_list<std::optional<bool>>{
             std::nullopt, false, true}) {
      config_->SetIsMachineExternallyManaged(is_managed);
      EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
      Component component(*update_context, "abc");
      component.crx_component_ = CrxComponent();
      component.crx_component_->app_id = "abc";
      component.previous_version_ = base::Version("1.0");
      component.AppendEvent(component.MakeEventUpdateComplete());
      ping_manager_->SendPing(component.session_id(), *component.crx_component_,
                              component.GetEvents(), MakePingCallback());

      RunThreads();

      ASSERT_EQ(interceptor->GetCount(), 1);
      const auto root = base::JSONReader::Read(interceptor->GetRequestBody(0));
      interceptor->Reset();

      ASSERT_TRUE(root);
      EXPECT_EQ(is_managed,
                root->GetDict().FindBoolByDottedPath("request.domainjoined"));
    }
  }

  {
    // Test `app_command_id`.
    CrxComponent crx_component;
    crx_component.app_id = "abc";
    crx_component.version = base::Version("1.2.3.4");

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    base::MakeRefCounted<UpdateEngine>(
        config_,
        base::BindRepeating(
            [](scoped_refptr<Configurator>) -> std::unique_ptr<UpdateChecker> {
              return nullptr;
            }),
        ping_manager_, base::DoNothing())
        ->SendPing(crx_component,
                   {
                       .event_type = protocol_request::kEventAppCommandComplete,
                       .result = false,
                       .error_code = -11,
                       .extra_code1 = 101,
                       .app_command_id = "appcommandid1",
                   },
                   base::BindLambdaForTesting([&](Error) { Quit(); }));
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
    EXPECT_EQ(false, event.FindInt("eventresult"));
    EXPECT_EQ(protocol_request::kEventAppCommandComplete,
              event.FindInt("eventtype"));
    EXPECT_EQ(-11, event.FindInt("errorcode"));
    EXPECT_EQ(101, event.FindInt("extracode1"));
    EXPECT_EQ("appcommandid1", CHECK_DEREF(event.FindString("appcommandid")));
    EXPECT_EQ("1.2.3.4", CHECK_DEREF(event.FindString("previousversion")));
    EXPECT_EQ(event.FindString("nextversion"), nullptr);

    interceptor->Reset();
  }

  config_->SetIsMachineExternallyManaged(std::nullopt);
}

// Tests that sending the ping fails when the component requires encryption but
// the ping URL is unsecure.
TEST_P(PingManagerTest, RequiresEncryption) {
  config_->SetPingUrl(GURL("http:\\foo\bar"));
  auto interceptor = std::make_unique<URLLoaderPostInterceptor>(
      config_->test_url_loader_factory());
  EXPECT_TRUE(interceptor);

  const auto update_context = MakeMockUpdateContext();
  {
    Component component(*update_context, "abc");
    component.crx_component_ = CrxComponent();
    component.crx_component_->app_id = "abc";
    component.crx_component_->version = base::Version("1.0");
    component.crx_component_->ap = "ap1";
    component.crx_component_->brand = "BRND";
    component.state_ = std::make_unique<Component::StateUpdated>(&component);
    component.previous_version_ = base::Version("1.0");
    component.next_version_ = base::Version("2.0");
    component.AppendEvent(component.MakeEventUpdateComplete());
    // The default value for |requires_network_encryption| is true.
    EXPECT_TRUE(component.crx_component_->requires_network_encryption);

    config_->GetPersistedData()->SetCohort("abc", "c1");
    config_->GetPersistedData()->SetCohortName("abc", "cn1");
    config_->GetPersistedData()->SetCohortHint("abc", "ch1");

    EXPECT_TRUE(interceptor->ExpectRequest(std::make_unique<AnyMatch>()));
    ping_manager_->SendPing(component.session_id(), *component.crx_component_,
                            component.GetEvents(), MakePingCallback());
    RunThreads();

    // Should not send
    EXPECT_EQ(0, interceptor->GetCount()) << interceptor->GetRequestsAsString();
  }
  config_->SetIsMachineExternallyManaged(std::nullopt);
}

}  // namespace update_client
