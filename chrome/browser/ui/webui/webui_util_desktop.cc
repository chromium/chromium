// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_util_desktop.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/widget.h"

namespace webui {

namespace {
const ui::ThemeProvider* g_theme_provider_for_testing = nullptr;

// Maps the url host to its metrics appropriate host name for metrics reporting.
// Keep in sync with the WebUIHostname variant in
// histograms/metadata/others.xml.
const std::string_view* GetWebUIMetricsHostname(const GURL& webui_url) {
  static constexpr auto kWebUIHostnames =
      base::MakeFixedFlatMap<std::string_view, std::string_view>({
          {chrome::kChromeUIBookmarksHost, "Bookmarks"},
          {chrome::kChromeUIBookmarksSidePanelHost, "BookmarksSidePanel"},
          {chrome::kChromeUICustomizeChromeSidePanelHost,
           "CustomizeChromeSidePanel"},
          {chrome::kChromeUIDownloadsHost, "Downloads"},
          {chrome::kChromeUIHistoryHost, "History"},
          {chrome::kChromeUIHistoryClustersSidePanelHost,
           "HistoryClustersSidePanel"},
          {chrome::kChromeUINewTabPageHost, "NewTabPage"},
          {chrome::kChromeUINewTabPageThirdPartyHost, "NewTabPageThirdParty"},
          {chrome::kChromeUIUntrustedReadAnythingSidePanelHost,
           "ReadAnythingSidePanel"},
          {chrome::kChromeUIReadLaterHost, "ReadLater"},
          {content::kChromeUIResourcesHost, "Resources"},
          {chrome::kChromeUISettingsHost, "Settings"},
          {chrome::kChromeUITabSearchHost, "TabSearch"},
          {chrome::kChromeUIThemeHost, "Theme"},
          {chrome::kChromeUITopChromeDomain, "TopChrome"},
      });
  return base::FindOrNull(kWebUIHostnames, webui_url.host());
}

}  // namespace

ui::NativeTheme* GetNativeThemeDeprecated(content::WebContents* web_contents) {
  ui::NativeTheme* native_theme = nullptr;

  if (web_contents) {
    Browser* browser = chrome::FindBrowserWithTab(web_contents);

    if (browser) {
      // Find for WebContents hosted in a tab.
      native_theme = browser->window()->GetNativeTheme();
    }

    if (!native_theme) {
      // Find for WebContents hosted in a widget, but not directly in a
      // Browser. e.g. Tab Search, Read Later.
      views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
          web_contents->GetContentNativeView());
      if (widget) {
        native_theme = widget->GetNativeTheme();
      }
    }
  }

  if (!native_theme) {
    // Find for isolated WebContents, e.g. in tests.
    // Or when |web_contents| is nullptr, because the renderer is not ready.
    // TODO(crbug.com/40677117): Remove global accessor to NativeTheme.
    native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  }

  return native_theme;
}

const ui::ThemeProvider* GetThemeProviderDeprecated(
    content::WebContents* web_contents) {
  if (g_theme_provider_for_testing) {
    return g_theme_provider_for_testing;
  }

  auto* browser_window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents);

  if (browser_window) {
    return browser_window->GetThemeProvider();
  }

  // Fallback 1: get the theme provider from the profile's associated browser.
  // This is used in newly created tabs, e.g. NewTabPageUI, where theming is
  // required before the WebContents is attached to a browser window.
  // TODO(crbug.com/40823135): Remove this fallback by associating the
  // WebContents during navigation.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (browser) {
    return browser->window()->GetThemeProvider();
  }

  // Fallback 2: get the theme provider from the last created browser.
  // This is used in ChromeOS, where under multi-signin a browser window can
  // be sent to another profile.
  // TODO(crbug.com/40823135): Remove this fallback by associating the
  // WebContents during navigation.
  BrowserList* browser_list = BrowserList::GetInstance();
  browser = browser_list->empty()
                ? nullptr
                : *std::prev(BrowserList::GetInstance()->end());
  return browser ? browser->window()->GetThemeProvider() : nullptr;
}

void SetThemeProviderForTestingDeprecated(
    const ui::ThemeProvider* theme_provider) {
  g_theme_provider_for_testing = theme_provider;
}

std::string GetWebUIHostnameForCodeCacheMetrics(const GURL& webui_url) {
  const std::string_view* hostname = GetWebUIMetricsHostname(webui_url);
  return hostname ? std::string(*hostname) : std::string();
}

}  // namespace webui
