// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/consent_kit/proto/iframe_interface.pb.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_search/pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kAccessToken[] = "access_token";
constexpr char kEmail[] = "test@example.com";

class MockDrivePickerBridge
    : public drive_picker_host_untrusted::mojom::DrivePickerBridge {
 public:
  MockDrivePickerBridge() = default;
  ~MockDrivePickerBridge() override = default;

  mojo::PendingRemote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(
      void,
      ShowDrivePicker,
      (mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>,
       drive_picker_host_untrusted::mojom::DrivePickerKeysPtr),
      (override));
  MOCK_METHOD(void, LoadConsentKitUrl, (const GURL&), (override));

 private:
  mojo::Receiver<drive_picker_host_untrusted::mojom::DrivePickerBridge>
      receiver_{this};
};

class MockResultHandler
    : public drive_picker_host::mojom::DrivePickerResultHandler {
 public:
  MockResultHandler() = default;
  ~MockResultHandler() override = default;

  mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnSelection,
              (std::vector<drive_picker_host::mojom::DriveFilePtr>),
              (override));
  MOCK_METHOD(void, OnCancel, (), (override));
  MOCK_METHOD(void,
              OnError,
              (drive_picker_host::mojom::DrivePickerError),
              (override));

 private:
  mojo::Receiver<drive_picker_host::mojom::DrivePickerResultHandler> receiver_{
      this};
};

}  // namespace

class DrivePickerHostUITest : public testing::Test {
 public:
  DrivePickerHostUITest() = default;
  ~DrivePickerHostUITest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactories(IdentityTestEnvironmentProfileAdaptor::
                                    GetIdentityTestEnvironmentFactories());
    profile_ = builder.Build();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  TestingProfile* profile() { return profile_.get(); }
  content::WebContents* web_contents() { return web_contents_.get(); }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DrivePickerHostUITest, IsWebUIEnabled_FeatureEnabled) {
  DrivePickerHostUIConfig config;
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);
  EXPECT_TRUE(config.IsWebUIEnabled(profile()));
}

TEST_F(DrivePickerHostUITest, IsWebUIEnabled_FeatureDisabled) {
  DrivePickerHostUIConfig config;
  feature_list_.InitAndDisableFeature(
      omnibox::kComposeboxDriveContextMenuOption);
  EXPECT_FALSE(config.IsWebUIEnabled(profile()));
}

TEST_F(DrivePickerHostUITest, TriggerDrivePickerHostForwardsToUntrusted) {
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  identity_test_env()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockDrivePickerBridge mock_bridge;
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  MockResultHandler result_handler;
  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      result_handler.BindAndGetRemote());

  base::RunLoop run_loop;
  EXPECT_CALL(mock_bridge, ShowDrivePicker(testing::_, testing::_))
      .WillOnce(
          [&run_loop](
              mojo::PendingRemote<
                  drive_picker_host::mojom::DrivePickerResultHandler>,
              drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys) {
            EXPECT_EQ(keys->oauth_token, kAccessToken);
            run_loop.Quit();
          });
  controller.TriggerDrivePickerHost(std::move(request));

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Now() + base::Hours(1));

  run_loop.Run();
}

TEST_F(DrivePickerHostUITest, TriggerDrivePickerHostQueuesUntilBridgeBound) {
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  identity_test_env()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockResultHandler result_handler;
  // Trigger before bridge is set.
  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      result_handler.BindAndGetRemote());
  controller.TriggerDrivePickerHost(std::move(request));

  MockDrivePickerBridge mock_bridge;
  // Setting bridge should flush the pending request and initiate the token
  // fetch.
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  // The call happens AFTER the token is fetched.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_bridge, ShowDrivePicker(testing::_, testing::_))
      .WillOnce(
          [&run_loop](
              mojo::PendingRemote<
                  drive_picker_host::mojom::DrivePickerResultHandler>,
              drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys) {
            EXPECT_EQ(keys->oauth_token, kAccessToken);
            run_loop.Quit();
          });

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Now() + base::Hours(1));

  run_loop.Run();
}

TEST_F(DrivePickerHostUITest, TriggerDrivePickerHostReportsTokenFetchFailure) {
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  identity_test_env()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockResultHandler result_handler;
  base::RunLoop run_loop;
  EXPECT_CALL(
      result_handler,
      OnError(drive_picker_host::mojom::DrivePickerError::kTokenFetchFailure))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      result_handler.BindAndGetRemote());
  controller.TriggerDrivePickerHost(std::move(request));

  MockDrivePickerBridge mock_bridge;
  // Binding the bridge should now trigger the fetch because a request is
  // pending.
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromServiceUnavailable(""));

  run_loop.Run();
}

TEST_F(DrivePickerHostUITest, LoadConsentKitUrl_ForwardsToUntrusted) {
  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockDrivePickerBridge mock_bridge;
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  const GURL test_url("https://consent.google.com/primitive?hl=en");
  base::RunLoop run_loop;
  EXPECT_CALL(mock_bridge, LoadConsentKitUrl(test_url))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  controller.LoadConsentKitUrl(test_url);

  run_loop.Run();
}

TEST_F(DrivePickerHostUITest, ConsentKitFlowSuccess_RedirectsToPicker) {
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  identity_test_env()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockDrivePickerBridge mock_bridge;
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  MockResultHandler result_handler;
  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kConsentDialog,
      result_handler.BindAndGetRemote());

  GURL consent_url;
  base::RunLoop consent_url_run_loop;
  EXPECT_CALL(mock_bridge, LoadConsentKitUrl(testing::_))
      .WillOnce([&consent_url, &consent_url_run_loop](const GURL& url) {
        consent_url = url;
        consent_url_run_loop.Quit();
      });

  controller.TriggerDrivePickerHost(std::move(request));
  consent_url_run_loop.Run();

  // Verify URL parameters
  EXPECT_TRUE(consent_url.is_valid());
  EXPECT_EQ(consent_url.host(), "consent.google.com");

  // Initial consent preference should not be kConsent
  EXPECT_NE(
      profile()->GetPrefs()->GetInteger(contextual_search::kDriveConsentState),
      static_cast<int>(contextual_search::DriveConsentState::kConsent));

  // Construct successful PrivacyFlowResult message
  identity_consent::IframeMessage message;
  message.set_event(identity_consent::Event::DECISION_RESPONSE_EVENT);
  message.mutable_privacy_flow_result()->mutable_flow_completed();
  message.mutable_privacy_flow_result()->set_flow_id(
      static_cast<identity_consent::ConsentFlowId>(
          omnibox::kComposeboxDriveConsentFlowId.Get()));
  auto* decision = message.mutable_privacy_flow_result()->add_decision();
  decision->set_ftc_consent_setting_id(
      identity_consent::ConsentSettingId::
          PERSONAL_CONTEXT_SEARCH_USING_WORKSPACE);
  decision->set_decision(identity_consent::Decision::DECISION_CONSENT);

  // Expect ShowDrivePicker to be called next
  base::RunLoop picker_run_loop;
  EXPECT_CALL(mock_bridge, ShowDrivePicker(testing::_, testing::_))
      .WillOnce(
          [&picker_run_loop](
              mojo::PendingRemote<
                  drive_picker_host::mojom::DrivePickerResultHandler>,
              drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys) {
            EXPECT_EQ(keys->oauth_token, kAccessToken);
            picker_run_loop.Quit();
          });

  // Simulate receiving successful message from iframe
  controller.OnConsentKitIframeMessage(mojo_base::ProtoWrapper(message));

  // Verify consent preference is updated to kConsent
  EXPECT_EQ(
      profile()->GetPrefs()->GetInteger(contextual_search::kDriveConsentState),
      static_cast<int>(contextual_search::DriveConsentState::kConsent));

  // Respond with access token to complete Picker setup
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Now() + base::Hours(1));

  picker_run_loop.Run();
}

TEST_F(DrivePickerHostUITest,
       ConsentKitFlowSuccess_KeepConsent_RedirectsToPicker) {
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  identity_test_env()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockDrivePickerBridge mock_bridge;
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  MockResultHandler result_handler;
  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kConsentDialog,
      result_handler.BindAndGetRemote());

  GURL consent_url;
  base::RunLoop consent_url_run_loop;
  EXPECT_CALL(mock_bridge, LoadConsentKitUrl(testing::_))
      .WillOnce([&consent_url, &consent_url_run_loop](const GURL& url) {
        consent_url = url;
        consent_url_run_loop.Quit();
      });

  controller.TriggerDrivePickerHost(std::move(request));
  consent_url_run_loop.Run();

  // Initial consent preference should not be kConsent
  EXPECT_NE(
      profile()->GetPrefs()->GetInteger(contextual_search::kDriveConsentState),
      static_cast<int>(contextual_search::DriveConsentState::kConsent));

  // Construct successful PrivacyFlowResult message with DECISION_KEEP_CONSENT
  identity_consent::IframeMessage message;
  message.set_event(identity_consent::Event::DECISION_RESPONSE_EVENT);
  message.mutable_privacy_flow_result()->mutable_flow_completed();
  message.mutable_privacy_flow_result()->set_flow_id(
      static_cast<identity_consent::ConsentFlowId>(
          omnibox::kComposeboxDriveConsentFlowId.Get()));
  auto* decision = message.mutable_privacy_flow_result()->add_decision();
  decision->set_ftc_consent_setting_id(
      identity_consent::ConsentSettingId::
          PERSONAL_CONTEXT_SEARCH_USING_WORKSPACE);
  decision->set_decision(identity_consent::Decision::DECISION_KEEP_CONSENT);

  // Expect ShowDrivePicker to be called next
  base::RunLoop picker_run_loop;
  EXPECT_CALL(mock_bridge, ShowDrivePicker(testing::_, testing::_))
      .WillOnce(
          [&picker_run_loop](
              mojo::PendingRemote<
                  drive_picker_host::mojom::DrivePickerResultHandler>,
              drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys) {
            EXPECT_EQ(keys->oauth_token, kAccessToken);
            picker_run_loop.Quit();
          });

  // Simulate receiving message from iframe
  controller.OnConsentKitIframeMessage(mojo_base::ProtoWrapper(message));

  // Verify consent preference is updated to kConsent
  EXPECT_EQ(
      profile()->GetPrefs()->GetInteger(contextual_search::kDriveConsentState),
      static_cast<int>(contextual_search::DriveConsentState::kConsent));

  // Respond with access token to complete Picker setup
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Now() + base::Hours(1));

  picker_run_loop.Run();
}

TEST_F(DrivePickerHostUITest, ConsentKitFlowNotCompleted_RelaysCancel) {
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockDrivePickerBridge mock_bridge;
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  MockResultHandler result_handler;
  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kConsentDialog,
      result_handler.BindAndGetRemote());

  base::RunLoop consent_url_run_loop;
  EXPECT_CALL(mock_bridge, LoadConsentKitUrl(testing::_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&consent_url_run_loop]() { consent_url_run_loop.Quit(); }));
  controller.TriggerDrivePickerHost(std::move(request));
  consent_url_run_loop.Run();

  // Construct flow_not_completed message with DECISION_ABANDONED.
  identity_consent::IframeMessage message;
  message.set_event(identity_consent::Event::DECISION_RESPONSE_EVENT);
  message.mutable_privacy_flow_result()->mutable_flow_not_completed();
  message.mutable_privacy_flow_result()->set_flow_id(
      static_cast<identity_consent::ConsentFlowId>(
          omnibox::kComposeboxDriveConsentFlowId.Get()));
  auto* decision = message.mutable_privacy_flow_result()->add_decision();
  decision->set_ftc_consent_setting_id(
      identity_consent::ConsentSettingId::
          PERSONAL_CONTEXT_SEARCH_USING_WORKSPACE);
  decision->set_decision(identity_consent::Decision::DECISION_ABANDONED);

  base::RunLoop cancel_run_loop;
  EXPECT_CALL(result_handler, OnCancel())
      .WillOnce(testing::InvokeWithoutArgs(
          [&cancel_run_loop]() { cancel_run_loop.Quit(); }));

  controller.OnConsentKitIframeMessage(mojo_base::ProtoWrapper(message));
  cancel_run_loop.Run();

  // Preference should remain not kConsent
  EXPECT_NE(
      profile()->GetPrefs()->GetInteger(contextual_search::kDriveConsentState),
      static_cast<int>(contextual_search::DriveConsentState::kConsent));
}

TEST_F(DrivePickerHostUITest, ConsentKitFlowCancel_RelaysCancel) {
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockDrivePickerBridge mock_bridge;
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  MockResultHandler result_handler;
  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kConsentDialog,
      result_handler.BindAndGetRemote());

  base::RunLoop consent_url_run_loop;
  EXPECT_CALL(mock_bridge, LoadConsentKitUrl(testing::_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&consent_url_run_loop]() { consent_url_run_loop.Quit(); }));
  controller.TriggerDrivePickerHost(std::move(request));
  consent_url_run_loop.Run();

  // Construct cancelled IframeMessage
  identity_consent::IframeMessage message;
  message.set_event(identity_consent::Event::TERMINATE_EVENT);

  base::RunLoop cancel_run_loop;
  // Expect OnCancel to be called on result handler
  EXPECT_CALL(result_handler, OnCancel())
      .WillOnce(testing::InvokeWithoutArgs(
          [&cancel_run_loop]() { cancel_run_loop.Quit(); }));

  controller.OnConsentKitIframeMessage(mojo_base::ProtoWrapper(message));
  cancel_run_loop.Run();

  // Preference should remain not kConsent
  EXPECT_NE(
      profile()->GetPrefs()->GetInteger(contextual_search::kDriveConsentState),
      static_cast<int>(contextual_search::DriveConsentState::kConsent));
}
