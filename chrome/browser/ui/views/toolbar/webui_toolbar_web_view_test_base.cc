// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_test_base.h"

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"

WebUIToolbarWebViewTestBase::WebUIToolbarWebViewTestBase()
    : WebUIToolbarWebViewTestBase(
          {features::kInitialWebUI, features::kWebUIReloadButton,
           features::kWebUISplitTabsButton, features::kWebUIHomeButton,
           features::kWebUIExtensionsContainer,
           features::kSkipIPCChannelPausingForNonGuests,
           features::kWebUIInProcessResourceLoadingV2,
           // Needed for browser_tests_no_field_trial.
           extensions_features::kExtensionsMenuAccessControl},
          {features::kExtensionsPinnedByDefault}) {}

WebUIToolbarWebViewTestBase::WebUIToolbarWebViewTestBase(
    const std::vector<base::test::FeatureRef>& enabled,
    const std::vector<base::test::FeatureRef>& disabled) {
  feature_list_.InitWithFeatures(enabled, disabled);
}

WebUIToolbarWebViewTestBase::~WebUIToolbarWebViewTestBase() = default;

void WebUIToolbarWebViewTestBase::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  ThemeServiceFactory::GetForProfile(browser()->GetProfile())
      ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
}

ToolbarView* WebUIToolbarWebViewTestBase::GetToolbarView() {
  return BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
}

void WebUIToolbarWebViewTestBase::SimulateDropOnToolbar(
    content::WebContents* web_contents,
    const std::string& text) {
  EXPECT_TRUE(content::ExecJs(web_contents, base::StringPrintf(R"(
    const toolbarApp = document.querySelector('toolbar-app');
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('text/plain', "%s");
    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      dataTransfer: dataTransfer
    });
    toolbarApp.dispatchEvent(dropEvent);
  )",
                                                               text.c_str())));
}

void WebUIToolbarWebViewTestBase::SimulateUriListDropOnToolbar(
    content::WebContents* web_contents,
    const std::string& url) {
  EXPECT_TRUE(content::ExecJs(web_contents, base::StringPrintf(R"(
    const toolbarApp = document.querySelector('toolbar-app');
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('text/uri-list', "%s");
    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      dataTransfer: dataTransfer
    });
    toolbarApp.dispatchEvent(dropEvent);
  )",
                                                               url.c_str())));
}

scoped_refptr<const extensions::Extension>
WebUIToolbarWebViewTestBase::LoadAndPinExtension(
    WebUIToolbarWebView* webui_toolbar_view,
    base::ScopedTempDir& temp_dir,
    bool has_background_script,
    bool has_popup) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtension(temp_dir, has_background_script, has_popup);
  if (!extension) {
    return nullptr;
  }

  // Pin the extension so it becomes visible.
  ToolbarActionsModel::Get(browser()->GetProfile())
      ->SetActionVisibility(extension->id(), true);

  base::RunLoop run_loop;
  webui_toolbar_view->extensions_container_.OnActionPoppedOut(
      run_loop.QuitClosure());
  run_loop.Run();

  return extension;
}

scoped_refptr<const extensions::Extension>
WebUIToolbarWebViewTestBase::LoadExtension(base::ScopedTempDir& temp_dir,
                                           bool has_background_script,
                                           bool has_popup) {
  base::FilePath manifest_path =
      temp_dir.GetPath().AppendASCII("manifest.json");

  std::string background_section = "";
  if (has_background_script) {
    background_section = R"(
      , "background": {
        "service_worker": "background.js"
      }
    )";
    base::FilePath script_path =
        temp_dir.GetPath().AppendASCII("background.js");
    std::string script_content = R"(
      chrome.action.onClicked.addListener(() => {
        chrome.test.sendMessage("clicked");
      });
    )";
    EXPECT_TRUE(base::WriteFile(script_path, script_content));
  }

  std::string action_section = "{}";
  if (has_popup) {
    action_section = R"({"default_popup": "popup.html"})";
    base::FilePath popup_path = temp_dir.GetPath().AppendASCII("popup.html");
    EXPECT_TRUE(base::WriteFile(popup_path, "<html><body>Popup</body></html>"));
  }

  std::string manifest_content =
      base::StringPrintf(R"({
    "name": "Test Extension",
    "version": "1.0",
    "manifest_version": 3,
    "action": %s
    %s,
    "host_permissions": ["*://allowed.com/*"]
  })",
                         action_section.c_str(), background_section.c_str());

  EXPECT_TRUE(base::WriteFile(manifest_path, manifest_content));

  extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(temp_dir.GetPath());
  EXPECT_TRUE(extension);

  return extension;
}
