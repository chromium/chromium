// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto service = std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
      *Profile::FromBrowserContext(context)->GetPrefs(),
      /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
      AimEligibilityService::Configuration{});
  ON_CALL(*service, IsAimEligible()).WillByDefault(testing::Return(true));
  ON_CALL(*service, GetLocaleImpl()).WillByDefault(testing::Return("en-US"));
  return service;
}

// Waits until the composebox renders its voice search element.
constexpr char kWaitForVoiceSearch[] = R"(
  new Promise(resolve => {
    const check = () => {
      const composebox = document.querySelector('omnibox-aim-app')
          ?.shadowRoot?.querySelector('#composebox');
      if (composebox?.shadowRoot?.querySelector('#voiceSearch')) {
        resolve(true);
      } else {
        setTimeout(check, 10);
      }
    };
    check();
  });
)";

// Clicks the voice search button of the composebox.
constexpr char kClickVoiceSearchButton[] = R"(
  document.querySelector('omnibox-aim-app').shadowRoot
      .querySelector('#composebox').onVoiceSearchButtonClick();
)";

// Starts speech recognition directly, and returns a promise that resolves
// with "started" or the error once the permission is decided.
constexpr char kStartRecognition[] = R"(
  window.recognition = new webkitSpeechRecognition();
  window.recognitionEnded = new Promise(resolve => {
    window.recognition.onend = resolve;
  });
  new Promise(resolve => {
    window.recognition.onstart = () => resolve('started');
    window.recognition.onerror = e => resolve(e.error);
    window.recognition.start();
  });
)";

constexpr char kAbortRecognition[] = R"(
  window.recognition.abort();
  window.recognitionEnded.then(() => true);
)";

}  // namespace

// Interactive, since the permission prompt is only shown while the popup is
// visible, and the popup closes when the browser window is deactivated.
class OmniboxAimPopupInteractiveTest : public InProcessBrowserTest {
 public:
  OmniboxAimPopupInteractiveTest() {
    feature_list_.InitWithFeatures({omnibox::internal::kWebUIOmniboxAimPopup,
                                    omnibox::internal::kWebUIOmniboxPopup},
                                   {features::kWebUILocationBar});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  AimEligibilityServiceFactory::GetInstance()
                      ->SetTestingFactory(
                          context,
                          base::BindRepeating(&BuildMockAimEligibilityService));
                }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        "us");
  }

 protected:
  LocationBarView* location_bar() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar_view();
  }

  OmniboxAimPopupWebUIContent* ShowPopupAndGetWebUIContent() {
    location_bar()
        ->GetOmniboxController()
        ->popup_state_manager()
        ->SetPopupState(OmniboxPopupState::kAim);
    auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
    EXPECT_TRUE(presenter);
    if (!presenter) {
      return nullptr;
    }
    presenter->Show();
    auto* content =
        static_cast<OmniboxAimPopupWebUIContent*>(presenter->GetWebUIContent());
    EXPECT_TRUE(content);
    return content;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

// Verifies that voice search in the AIM popup prompts the user for microphone
// access and persists the grant in the browser window's user profile, even
// when the popup WebContents is hosted in `TopChromeWebUIProfile`.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupInteractiveTest,
                       VoiceSearchMicrophonePermissionUsesUserProfile) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  auto* content = ShowPopupAndGetWebUIContent();
  ASSERT_TRUE(content);
  content::WebContents* popup = content->GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(popup));

  auto* manager = permissions::PermissionRequestManager::FromWebContents(popup);
  ASSERT_TRUE(manager);
  permissions::MockPermissionPromptFactory prompt_factory(manager);
  prompt_factory.set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // The grant is stored for the canonical origin of the popup, i.e. the
  // origin of the default search engine.
  const GURL popup_url =
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin().GetURL();
  const GURL url = permissions::PermissionUtil::GetCanonicalOrigin(
      ContentSettingsType::MEDIASTREAM_MIC, popup_url, popup_url);
  HostContentSettingsMap* user_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(CONTENT_SETTING_ALLOW,
            user_settings_map->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_MIC));

  ASSERT_EQ(true, content::EvalJs(popup, kWaitForVoiceSearch));
  ASSERT_TRUE(content::ExecJs(popup, kClickVoiceSearchButton));
  ASSERT_TRUE(
      base::test::RunUntil([&] { return prompt_factory.show_count() == 1; }));
  EXPECT_TRUE(base::test::RunUntil([&] {
    return user_settings_map->GetContentSetting(
               url, url, ContentSettingsType::MEDIASTREAM_MIC) ==
           CONTENT_SETTING_ALLOW;
  }));

  // A subsequent voice search should use the persisted grant on the user
  // profile without showing another prompt.
  EXPECT_NE("not-allowed", content::EvalJs(popup, kStartRecognition));
  EXPECT_EQ(true, content::EvalJs(popup, kAbortRecognition));
  EXPECT_EQ(1, prompt_factory.show_count());
}
