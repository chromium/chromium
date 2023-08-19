// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_util.h"

#include "base/containers/cxx20_erase.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/enterprise_util.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/widget.h"
#endif  // defined(TOOLKIT_VIEWS)

namespace webui {

void SetJSModuleDefaults(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test "
#if BUILDFLAG(IS_CHROMEOS_ASH)
      "chrome://test "
#endif
      "'self';");

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
}

void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const ResourcePath> resources,
                          int default_resource) {
  SetJSModuleDefaults(source);
  EnableTrustedTypesCSP(source);
  source->AddResourcePaths(resources);
  source->AddResourcePath("", default_resource);
}

// There is another method, ash::EnableTrustedTypesCSP, used by ash-only WebUIs.
// When adding a new policy here, consider whether to add it to that method as
// well, as these methods should remain mostly the same.
void EnableTrustedTypesCSP(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "require-trusted-types-for 'script';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types parse-html-subset sanitize-inner-html static-types "
      // Add TrustedTypes policies for cr-lottie.
      "lottie-worker-script-loader "
      // Add TrustedTypes policies used during tests.
      "webui-test-script webui-test-html "
      // Add TrustedTypes policy for creating the PDF plugin.
      "print-preview-plugin-loader "
      // Add TrustedTypes policies necessary for using Polymer.
      "polymer-html-literal polymer-template-event-attribute-policy;");
}

void AddLocalizedString(content::WebUIDataSource* source,
                        const std::string& message,
                        int id) {
  std::u16string str = l10n_util::GetStringUTF16(id);
  base::Erase(str, '&');
  source->AddString(message, str);
}

void SetupChromeRefresh2023(content::WebUIDataSource* source) {
  source->AddString(
      "chromeRefresh2023Attribute",
      features::IsChromeWebuiRefresh2023() ? "chrome-refresh-2023" : "");
}

#if defined(TOOLKIT_VIEWS)

namespace {
const ui::ThemeProvider* g_theme_provider_for_testing = nullptr;
}  // namespace

ui::NativeTheme* GetNativeTheme(content::WebContents* web_contents) {
  ui::NativeTheme* native_theme = nullptr;

  if (web_contents) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

    if (browser) {
      // Find for WebContents hosted in a tab.
      native_theme = browser->window()->GetNativeTheme();
    }

    if (!native_theme) {
      // Find for WebContents hosted in a widget, but not directly in a
      // Browser. e.g. Tab Search, Read Later.
      views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
          web_contents->GetContentNativeView());
      if (widget)
        native_theme = widget->GetNativeTheme();
    }
  }

  if (!native_theme) {
    // Find for isolated WebContents, e.g. in tests.
    // Or when |web_contents| is nullptr, because the renderer is not ready.
    // TODO(crbug/1056916): Remove global accessor to NativeTheme.
    native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  }

  return native_theme;
}

const ui::ThemeProvider* GetThemeProvider(content::WebContents* web_contents) {
  if (g_theme_provider_for_testing)
    return g_theme_provider_for_testing;

  auto* browser_window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents);

  if (browser_window)
    return browser_window->GetThemeProvider();

  // Fallback 1: get the theme provider from the profile's associated browser.
  // This is used in newly created tabs, e.g. NewTabPageUI, where theming is
  // required before the WebContents is attached to a browser window.
  // TODO(crbug.com/1298767): Remove this fallback by associating the
  // WebContents during navigation.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (browser)
    return browser->window()->GetThemeProvider();

  // Fallback 2: get the theme provider from the last created browser.
  // This is used in ChromeOS, where under multi-signin a browser window can
  // be sent to another profile.
  // TODO(crbug.com/1298767): Remove this fallback by associating the
  // WebContents during navigation.
  BrowserList* browser_list = BrowserList::GetInstance();
  browser = browser_list->empty()
                ? nullptr
                : *std::prev(BrowserList::GetInstance()->end());
  return browser ? browser->window()->GetThemeProvider() : nullptr;
}

void SetThemeProviderForTesting(const ui::ThemeProvider* theme_provider) {
  g_theme_provider_for_testing = theme_provider;
}

#endif  // defined(TOOLKIT_VIEWS)

}  // namespace webui
