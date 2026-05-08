// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view_class_properties.h"

namespace {

WebUIToolbarWebView* GetWebUIToolbarWebView(Browser* browser) {
  return static_cast<ToolbarButtonProvider*>(
             BrowserView::GetBrowserViewForBrowser(browser)->toolbar())
      ->GetWebUIToolbarViewForTesting();
}

std::string GetContentSettingIconJS(
    toolbar_ui_api::mojom::ContentSettingImageType type) {
  return base::StringPrintf(R"(
    ((type) => {
      const app = document.querySelector('toolbar-app');
      const locBar = app.shadowRoot.querySelector('#location-bar');
      const contentSettings = locBar.shadowRoot
                              .querySelector('#contentSettings');
      const icon = Array.from(contentSettings.shadowRoot
                      .querySelectorAll('content-setting-icon'))
                  .find(el => el.state && el.state.type === type);
      return icon;
    })(%d)
  )",
                            static_cast<int>(type));
}

}  // namespace

class WebUIToolbarWebViewContentSettingsInteractiveTest
    : public InteractiveBrowserTest {
 public:
  WebUIToolbarWebViewContentSettingsInteractiveTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUILocationBar,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void TriggerContentBlocked(content::WebContents* web_contents,
                             ContentSettingsType type) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    HostContentSettingsMapFactory::GetForProfile(profile)
        ->SetDefaultContentSetting(type, CONTENT_SETTING_BLOCK);

    auto* settings = content_settings::PageSpecificContentSettings::GetForFrame(
        web_contents->GetPrimaryMainFrame());
    ASSERT_TRUE(settings);
    settings->OnContentBlocked(type);

    WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
    webui_toolbar_view->GetLocationBar()->UpdateContentSettingsIcons();
  }

  GURL GetTestURL() {
    return embedded_test_server()->GetURL("foo.test", "/title1.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewContentSettingsInteractiveTest,
                       OpenBubble) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);
  const auto type = toolbar_ui_api::mojom::ContentSettingImageType::kCookies;

  RunTestSequence(
      InstrumentTab(kActiveTabId), WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId),
      NavigateWebContents(kActiveTabId, GetTestURL()), Do([this]() {
        TriggerContentBlocked(
            browser()->tab_strip_model()->GetActiveWebContents(),
            ContentSettingsType::COOKIES);
      }),
      WaitForJsResult(kWebUIToolbarWebViewId,
                      base::StringPrintf(R"(
        () => {
          const icon = %s;
          return !!icon && icon.checkVisibility();
        }
      )",
                                         GetContentSettingIconJS(type).c_str()),
                      true),
      ExecuteJsAt(kWebUIToolbarWebViewId, DeepQuery{},
                  base::StringPrintf(R"(
        () => {
          const icon = %s;
          const button = icon.shadowRoot.querySelector('cr-icon-button');
          button.click();
        }
      )",
                                     GetContentSettingIconJS(type).c_str())),
      WaitForShow(ContentSettingBubbleContents::kMainElementId),
      CheckView(ContentSettingBubbleContents::kMainElementId,
                [](ContentSettingBubbleContents* bubble) {
                  return bubble->get_message_for_test() ==
                         l10n_util::GetStringUTF16(
                             IDS_BLOCKED_ON_DEVICE_SITE_DATA_MESSAGE);
                }));
}
