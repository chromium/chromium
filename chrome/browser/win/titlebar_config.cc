// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/titlebar_config.h"

#include "base/win/windows_version.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/native_theme/native_theme.h"

// Allows the titlebar to be drawn by the system using the Mica material
// on Windows 11, version 22H2 and above.
BASE_FEATURE(kWindows11MicaTitlebar,
             "Windows11MicaTitlebar",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldBrowserCustomDrawTitlebar(BrowserView* browser_view) {
  return !ShouldDefaultThemeUseMicaTitlebar() ||
         !ThemeServiceFactory::GetForProfile(browser_view->GetProfile())
              ->UsingSystemTheme() ||
         (!browser_view->browser()->is_type_normal() &&
          !browser_view->browser()->is_type_popup() &&
          !browser_view->browser()->is_type_devtools());
}

bool ShouldDefaultThemeUseMicaTitlebar() {
  return SystemTitlebarCanUseMicaMaterial() &&
         !ui::AccentColorObserver::Get()->accent_color().has_value() &&
         !ui::NativeTheme::GetInstanceForNativeUi()
              ->UserHasContrastPreference();
}

bool SystemTitlebarCanUseMicaMaterial() {
  return base::win::GetVersion() >= base::win::Version::WIN11_22H2 &&
         base::FeatureList::IsEnabled(kWindows11MicaTitlebar);
}
