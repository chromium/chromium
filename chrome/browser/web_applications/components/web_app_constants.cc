// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_constants.h"

namespace web_app {

static_assert(Source::kMinValue == 0, "Source enum should be zero based");

bool IsSuccess(InstallResultCode code) {
  return code == InstallResultCode::kSuccessNewInstall ||
         code == InstallResultCode::kSuccessAlreadyInstalled;
}

DisplayMode ResolveEffectiveDisplayMode(DisplayMode app_display_mode,
                                        DisplayMode user_display_mode) {
  switch (user_display_mode) {
    case DisplayMode::kBrowser:
      return DisplayMode::kBrowser;
    case DisplayMode::kUndefined:
    case DisplayMode::kMinimalUi:
    case DisplayMode::kFullscreen:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
      break;
  }

  switch (app_display_mode) {
    case DisplayMode::kBrowser:
    case DisplayMode::kMinimalUi:
      return DisplayMode::kMinimalUi;
    case DisplayMode::kUndefined:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
    case DisplayMode::kFullscreen:
      return DisplayMode::kStandalone;
  }
}

}  // namespace web_app
