// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin_internals_ui.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_debug_info.h"
#include "chrome/browser/signin/bound_session_credentials/mock_bound_session_cookie_refresh_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "google_apis/gaia/gaia_urls.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/device_bound_sessions/cookie_craving_display.h"
#include "net/device_bound_sessions/deletion_reason.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "net/device_bound_sessions/session_display.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_event.h"
#include "net/device_bound_sessions/session_inclusion_rules_display.h"
#include "net/device_bound_sessions/session_key.h"
#include "services/network/test/mock_device_bound_session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::base::test::DictionaryHasValues;
using ::net::device_bound_sessions::CookieCravingDisplay;
using ::net::device_bound_sessions::DeletionReason;
using ::net::device_bound_sessions::RefreshResult;
using ::net::device_bound_sessions::SessionDisplay;
using ::net::device_bound_sessions::SessionError;
using ::net::device_bound_sessions::SessionEvent;
using ::net::device_bound_sessions::SessionInclusionRulesDisplay;
using ::net::device_bound_sessions::SessionKey;
using ::testing::ElementsAre;

net::SchemefulSite GoogleSite() {
  return net::SchemefulSite(GaiaUrls::GetInstance()->gaia_url());
}

CookieCravingDisplay CreateCookieCravingDisplay(
    const std::string& name = "1P_JAR",
    const std::string& domain = ".google.com",
    const std::string& path = "/") {
  return CookieCravingDisplay(name, domain, path, /*secure=*/true,
                              /*http_only=*/true,
                              net::CookieSameSite::NO_RESTRICTION);
}

SessionDisplay CreateSessionDisplay(
    const net::SchemefulSite& site = GoogleSite(),
    const std::string& session_id = "google_session_id",
    const GURL& refresh_url =
        GURL("https://accounts.google.com/RotateBoundCookies"),
    std::vector<CookieCravingDisplay> cookie_cravings = {
        CreateCookieCravingDisplay()}) {
  return SessionDisplay(
      SessionKey{site, SessionKey::Id(session_id)}, refresh_url,
      SessionInclusionRulesDisplay(), std::move(cookie_cravings),
      base::Time::Now() + base::Days(1),
      /*cached_challenge=*/std::nullopt, /*allowed_refresh_initiators=*/{});
}

}  // namespace

class SignInInternalsHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    if (base::FeatureList::IsEnabled(
            switches::kEnableBoundSessionCredentials) &&
        !base::FeatureList::IsEnabled(
            switches::kBoundSessionCredentialsKillSwitch)) {
      profile_builder.AddTestingFactory(
          BoundSessionCookieRefreshServiceFactory::GetInstance(),
          base::BindOnce([](content::BrowserContext*) {
            return MockBoundSessionCookieRefreshService::Build();
          }));
    }
    profile_ = profile_builder.Build();

    if (base::FeatureList::IsEnabled(net::features::kDeviceBoundSessions)) {
      auto mock_session_manager = std::make_unique<
          testing::NiceMock<network::MockDeviceBoundSessionManager>>();
      ON_CALL(*mock_session_manager, AddEventObserver)
          .WillByDefault([this](mojo::PendingRemote<
                                network::mojom::DeviceBoundSessionEventObserver>
                                    observer) {
            observer_remote_.Bind(std::move(observer));
          });
      profile_->GetDefaultStoragePartition()
          ->OverrideDeviceBoundSessionManagerForTesting(
              std::move(mock_session_manager));
    }

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    web_ui_.set_web_contents(web_contents_.get());

    auto handler = std::make_unique<SignInInternalsHandler>();
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
    handler_->AllowJavascriptForTesting();
    task_environment_.RunUntilIdle();
  }

  MockBoundSessionCookieRefreshService* GetMockBoundSessionService() {
    return static_cast<MockBoundSessionCookieRefreshService*>(
        BoundSessionCookieRefreshServiceFactory::GetForProfile(profile_.get()));
  }

  // Extracts the "boundSessionInfo" list from the most recent push update sent
  // via `FireWebUIListener("signin-info-changed", signin_status)`.
  // `FireWebUIListener` invokes `cr.webUIListenerCallback(event_name, data)`:
  //   - arg1(): event_name ("signin-info-changed")
  //   - arg2(): data (the `signin_status` dictionary)
  const base::ListValue* GetLatestBoundSessionInfoFromListener() {
    for (const auto& call_data_ptr : base::Reversed(web_ui_.call_data())) {
      const content::TestWebUI::CallData& call_data = *call_data_ptr;
      if (call_data.function_name() == "cr.webUIListenerCallback" &&
          call_data.arg1() && call_data.arg1()->is_string() &&
          call_data.arg1()->GetString() == "signin-info-changed" &&
          call_data.arg2() && call_data.arg2()->is_dict()) {
        return call_data.arg2()->GetDict().FindList("boundSessionInfo");
      }
    }
    return nullptr;
  }

  // Extracts the "boundSessionInfo" list from the response to a
  // `chrome.send("getSigninInfo", [callback_id])` request sent via
  // `ResolveJavascriptCallback(callback_id, signin_status)`.
  // `ResolveJavascriptCallback` invokes
  // `cr.webUIResponse(callback_id, is_success, data)`:
  //   - arg1(): callback_id
  //   - arg2(): is_success (boolean)
  //   - arg3(): data (the `signin_status` dictionary)
  const base::ListValue* GetLatestBoundSessionInfoFromCallback(
      std::string_view callback_id) {
    for (const auto& call_data_ptr : base::Reversed(web_ui_.call_data())) {
      const content::TestWebUI::CallData& call_data = *call_data_ptr;
      if (call_data.function_name() == "cr.webUIResponse" && call_data.arg1() &&
          call_data.arg1()->is_string() &&
          call_data.arg1()->GetString() == callback_id && call_data.arg3() &&
          call_data.arg3()->is_dict()) {
        return call_data.arg3()->GetDict().FindList("boundSessionInfo");
      }
    }
    return nullptr;
  }

  // Extracts the "boundSessionFeatureStatus" list (containing the feature
  // status rows for Prototype and Standard DBSC) from the response to a
  // `chrome.send("getSigninInfo", [callback_id])` request sent via
  // `ResolveJavascriptCallback(callback_id, signin_status)`.
  // `ResolveJavascriptCallback` invokes
  // `cr.webUIResponse(callback_id, is_success, data)`:
  //   - arg1(): callback_id
  //   - arg2(): is_success (boolean)
  //   - arg3(): data (the `signin_status` dictionary)
  const base::ListValue* GetLatestBoundSessionFeatureStatusFromCallback(
      std::string_view callback_id) {
    for (const auto& call_data_ptr : base::Reversed(web_ui_.call_data())) {
      const content::TestWebUI::CallData& call_data = *call_data_ptr;
      if (call_data.function_name() == "cr.webUIResponse" && call_data.arg1() &&
          call_data.arg1()->is_string() &&
          call_data.arg1()->GetString() == callback_id && call_data.arg3() &&
          call_data.arg3()->is_dict()) {
        return call_data.arg3()->GetDict().FindList(
            "boundSessionFeatureStatus");
      }
    }
    return nullptr;
  }

  base::test::ScopedFeatureList feature_list_{
      /*enabled_features=*/{switches::kEnableBoundSessionCredentials,
                            net::features::kDeviceBoundSessions}};
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  raw_ptr<SignInInternalsHandler> handler_ = nullptr;
  mojo::Remote<network::mojom::DeviceBoundSessionEventObserver>
      observer_remote_;
};

TEST_F(SignInInternalsHandlerTest,
       AddDeviceBoundSessionDisplaysFiltersByGoogleSite) {
  SessionDisplay google_session = CreateSessionDisplay(
      GoogleSite(), "google_session_id",
      GURL("https://accounts.google.com/RotateBoundCookies"),
      {CreateCookieCravingDisplay("1P_JAR", ".google.com", "/"),
       CreateCookieCravingDisplay("SID", ".google.com", "/")});
  SessionDisplay non_google_session = CreateSessionDisplay(
      net::SchemefulSite(GURL("https://youtube.com")), "youtube_session_id",
      GURL("https://youtube.com/RotateBoundCookies"),
      {CreateCookieCravingDisplay("VISITOR_INFO", ".youtube.com", "/")});

  handler_->AddDeviceBoundSessionDisplays({google_session, non_google_session});

  const base::ListValue* bound_sessions =
      GetLatestBoundSessionInfoFromListener();
  ASSERT_NE(bound_sessions, nullptr);
  EXPECT_THAT(*bound_sessions,
              ElementsAre(DictionaryHasValues(
                  base::DictValue()
                      .Set("sessionID", "google_session_id")
                      .Set("type", "Standard")
                      .Set("domain", "google.com")
                      .Set("path", "/")
                      .Set("boundCookieNames", "1P_JAR, SID")
                      .Set("refreshUrl",
                           "https://accounts.google.com/RotateBoundCookies"))));
  const base::DictValue& session_dict = (*bound_sessions)[0].GetDict();
  EXPECT_EQ(session_dict.FindString("expirationTime"), nullptr);
  EXPECT_EQ(session_dict.FindBool("throttlingPaused"), std::nullopt);
}

TEST_F(SignInInternalsHandlerTest,
       OnDeviceBoundSessionEventReceivedHandlesLifecycleEvents) {
  // 1. Creation event adds a google.com session.
  SessionDisplay initial_display =
      CreateSessionDisplay(GoogleSite(), "standard_session_1");
  handler_->OnDeviceBoundSessionEventReceived(SessionEvent::MakeCreationEvent(
      GoogleSite(), "standard_session_1", /*succeeded=*/true,
      SessionError(SessionError::ErrorType::kSuccess), initial_display));

  const base::ListValue* bound_sessions =
      GetLatestBoundSessionInfoFromListener();
  ASSERT_NE(bound_sessions, nullptr);
  EXPECT_THAT(*bound_sessions, ElementsAre(DictionaryHasValues(
                                   base::DictValue()
                                       .Set("sessionID", "standard_session_1")
                                       .Set("boundCookieNames", "1P_JAR"))));

  // 2. Refresh event updates the session config.
  web_ui_.ClearTrackedCalls();
  SessionDisplay updated_display = CreateSessionDisplay(
      GoogleSite(), "standard_session_1",
      GURL("https://accounts.google.com/RotateBoundCookiesV2"),
      {CreateCookieCravingDisplay("1P_JAR", ".google.com", "/"),
       CreateCookieCravingDisplay("SAPISID", ".google.com", "/auth")});
  handler_->OnDeviceBoundSessionEventReceived(SessionEvent::MakeRefreshEvent(
      GoogleSite(), "standard_session_1", /*succeeded=*/true,
      RefreshResult::kRefreshed, /*fetch_error=*/std::nullopt, updated_display,
      /*was_fully_proactive_refresh=*/false));

  bound_sessions = GetLatestBoundSessionInfoFromListener();
  ASSERT_NE(bound_sessions, nullptr);
  EXPECT_THAT(
      *bound_sessions,
      ElementsAre(DictionaryHasValues(
          base::DictValue()
              .Set("sessionID", "standard_session_1")
              .Set("domain", "google.com")
              .Set("path", "/, /auth")
              .Set("boundCookieNames", "1P_JAR, SAPISID")
              .Set("refreshUrl",
                   "https://accounts.google.com/RotateBoundCookiesV2"))));

  // 3. Termination event removes the session and falls back to the placeholder.
  web_ui_.ClearTrackedCalls();
  handler_->OnDeviceBoundSessionEventReceived(
      SessionEvent::MakeTerminationEvent(GoogleSite(), "standard_session_1",
                                         /*succeeded=*/true,
                                         DeletionReason::kServerRequested));

  bound_sessions = GetLatestBoundSessionInfoFromListener();
  ASSERT_NE(bound_sessions, nullptr);
  EXPECT_THAT(*bound_sessions,
              ElementsAre(DictionaryHasValues(base::DictValue().Set(
                  "sessionID", "No active bound sessions."))));
  EXPECT_EQ((*bound_sessions)[0].GetDict().FindString("type"), nullptr);
}

TEST_F(SignInInternalsHandlerTest, DisplaysBothPrototypeAndStandardSessions) {
  EXPECT_CALL(*GetMockBoundSessionService(), GetBoundSessionDebugInfo())
      .WillRepeatedly([]() {
        std::vector<BoundSessionDebugInfo> info;
        info.emplace_back(
            "prototype_session_1", "google.com", "/",
            /*throttling_paused=*/false, base::Time::Now() + base::Hours(1),
            "__Secure-1PSIDTS, __Secure-3PSIDTS",
            GURL("https://accounts.google.com/RotateBoundCookiesProto"));
        return info;
      });

  SessionDisplay standard_session = CreateSessionDisplay(
      GoogleSite(), "standard_session_1",
      GURL("https://accounts.google.com/RotateBoundCookiesStandard"),
      {CreateCookieCravingDisplay("__Secure-1PSIDTS", ".google.com", "/")});
  handler_->AddDeviceBoundSessionDisplays({standard_session});

  const base::ListValue* bound_sessions =
      GetLatestBoundSessionInfoFromListener();
  ASSERT_NE(bound_sessions, nullptr);
  EXPECT_THAT(
      *bound_sessions,
      ElementsAre(
          DictionaryHasValues(
              base::DictValue()
                  .Set("sessionID", "prototype_session_1")
                  .Set("type", "Prototype")
                  .Set("domain", "google.com")
                  .Set("path", "/")
                  .Set("throttlingPaused", false)
                  .Set("boundCookieNames", "__Secure-1PSIDTS, __Secure-3PSIDTS")
                  .Set("refreshUrl",
                       "https://accounts.google.com/RotateBoundCookiesProto")),
          DictionaryHasValues(base::DictValue()
                                  .Set("sessionID", "standard_session_1")
                                  .Set("type", "Standard")
                                  .Set("domain", "google.com")
                                  .Set("path", "/")
                                  .Set("boundCookieNames", "__Secure-1PSIDTS")
                                  .Set("refreshUrl",
                                       "https://accounts.google.com/"
                                       "RotateBoundCookiesStandard"))));
  const base::DictValue& standard_dict = (*bound_sessions)[1].GetDict();
  EXPECT_EQ(standard_dict.FindString("expirationTime"), nullptr);
  EXPECT_EQ(standard_dict.FindBool("throttlingPaused"), std::nullopt);
}

TEST_F(SignInInternalsHandlerTest, IgnoresFailedAndNonGoogleEvents) {
  web_ui_.ClearTrackedCalls();

  // 1. Non-Google creation event is ignored.
  net::SchemefulSite youtube_site(GURL("https://youtube.com"));
  SessionDisplay youtube_display =
      CreateSessionDisplay(youtube_site, "youtube_session");
  handler_->OnDeviceBoundSessionEventReceived(SessionEvent::MakeCreationEvent(
      youtube_site, "youtube_session", /*succeeded=*/true,
      SessionError(SessionError::ErrorType::kSuccess), youtube_display));
  EXPECT_EQ(GetLatestBoundSessionInfoFromListener(), nullptr);

  // 2. Failed Google creation event is ignored.
  SessionDisplay google_display = CreateSessionDisplay();
  handler_->OnDeviceBoundSessionEventReceived(SessionEvent::MakeCreationEvent(
      GoogleSite(), "google_session_id", /*succeeded=*/false,
      SessionError(SessionError::ErrorType::kNetError), google_display));
  EXPECT_EQ(GetLatestBoundSessionInfoFromListener(), nullptr);
}

TEST_F(SignInInternalsHandlerTest,
       MojoObserverRegistrationDisconnectionAndGetSigninInfo) {
  ASSERT_TRUE(observer_remote_.is_bound());

  observer_remote_->AddDeviceBoundSessionDisplays({CreateSessionDisplay()});
  observer_remote_.FlushForTesting();

  // Verify `getSigninInfo` returns the populated standard session.
  web_ui_.ClearTrackedCalls();
  base::ListValue args;
  args.Append("test_callback_id");
  web_ui_.HandleReceivedMessage("getSigninInfo", args);

  const base::ListValue* bound_sessions =
      GetLatestBoundSessionInfoFromCallback("test_callback_id");
  ASSERT_NE(bound_sessions, nullptr);
  EXPECT_THAT(*bound_sessions,
              ElementsAre(DictionaryHasValues(
                  base::DictValue().Set("sessionID", "google_session_id"))));

  // Simulating a Network Service crash / Mojo disconnect clears the cached
  // standard sessions and notifies the UI.
  web_ui_.ClearTrackedCalls();
  observer_remote_.reset();
  task_environment_.RunUntilIdle();

  bound_sessions = GetLatestBoundSessionInfoFromListener();
  ASSERT_NE(bound_sessions, nullptr);
  EXPECT_THAT(*bound_sessions,
              ElementsAre(DictionaryHasValues(base::DictValue().Set(
                  "sessionID", "No active bound sessions."))));
}
TEST_F(SignInInternalsHandlerTest,
       DisplaysBoundSessionFeatureStatusWhenEnabled) {
  base::test::ScopedFeatureList scoped_features(
      /*enabled_features=*/
      {switches::kEnableBoundSessionCredentials,
       switches::kEnableOAuthMultiloginCookiesBinding,
       net::features::kDeviceBoundSessions,
       net::features::kDeviceBoundSessionsForRestrictedSites,
       switches::kEnableOAuthMultiloginStandardCookiesBinding});

  web_ui_.ClearTrackedCalls();
  base::ListValue args;
  args.Append("callback_enabled");
  web_ui_.HandleReceivedMessage("getSigninInfo", args);

  const base::ListValue* feature_status =
      GetLatestBoundSessionFeatureStatusFromCallback("callback_enabled");
  ASSERT_NE(feature_status, nullptr);
  EXPECT_THAT(
      *feature_status,
      ElementsAre(
          DictionaryHasValues(base::DictValue()
                                  .Set("type", "Prototype")
                                  .Set("serviceEnabled", true)
                                  .Set("googleRegistrationsEnabled", true)
                                  .Set("multiloginBindingEnabled", true)),
          DictionaryHasValues(base::DictValue()
                                  .Set("type", "Standard")
                                  .Set("serviceEnabled", true)
                                  .Set("googleRegistrationsEnabled", true)
                                  .Set("multiloginBindingEnabled", true))));
}

class SignInInternalsHandlerDisabledServicesTest
    : public SignInInternalsHandlerTest {
 private:
  base::test::ScopedFeatureList scoped_features_{
      /*enabled_features=*/
      {switches::kEnableBoundSessionCredentials,
       switches::kBoundSessionCredentialsKillSwitch,
       switches::kEnableOAuthMultiloginCookiesBinding,
       net::features::kDeviceBoundSessionsForRestrictedSites,
       switches::kEnableOAuthMultiloginStandardCookiesBinding},
      /*disabled_features=*/{net::features::kDeviceBoundSessions}};
};

TEST_F(SignInInternalsHandlerDisabledServicesTest,
       DisplaysBoundSessionFeatureStatusWhenServicesDisabled) {
  // When both services are disabled (`kBoundSessionCredentialsKillSwitch`
  // enabled for Prototype and `kDeviceBoundSessions` disabled for Standard),
  // `serviceEnabled` and all dependent flags report `false`.
  web_ui_.ClearTrackedCalls();
  base::ListValue args;
  args.Append("callback_disabled");
  web_ui_.HandleReceivedMessage("getSigninInfo", args);

  const base::ListValue* feature_status =
      GetLatestBoundSessionFeatureStatusFromCallback("callback_disabled");
  ASSERT_NE(feature_status, nullptr);
  EXPECT_THAT(
      *feature_status,
      ElementsAre(
          DictionaryHasValues(base::DictValue()
                                  .Set("type", "Prototype")
                                  .Set("serviceEnabled", false)
                                  .Set("googleRegistrationsEnabled", false)
                                  .Set("multiloginBindingEnabled", false)),
          DictionaryHasValues(base::DictValue()
                                  .Set("type", "Standard")
                                  .Set("serviceEnabled", false)
                                  .Set("googleRegistrationsEnabled", false)
                                  .Set("multiloginBindingEnabled", false))));
}
