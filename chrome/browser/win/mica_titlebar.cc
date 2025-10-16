// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/mica_titlebar.h"

#include <optional>

#include "base/win/windows_version.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/native_theme/native_theme.h"

// Allows the titlebar to be drawn by the system using the Mica material
// on Windows 11, version 22H2 and above.
BASE_FEATURE(kWindows11MicaTitlebar, base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldDefaultThemeUseMicaTitlebar() {
  return SystemTitlebarCanUseMicaMaterial() &&
         !ui::AccentColorObserver::Get()
              ->ShouldUseAccentColorForWindowFrame() &&
         ui::NativeTheme::GetInstanceForNativeUi()->preferred_contrast() ==
             ui::NativeTheme::PreferredContrast::kNoPreference;
}

bool SystemTitlebarCanUseMicaMaterial() {
  return base::win::GetVersion() >= base::win::Version::WIN11_22H2 &&
         base::FeatureList::IsEnabled(kWindows11MicaTitlebar);
}
