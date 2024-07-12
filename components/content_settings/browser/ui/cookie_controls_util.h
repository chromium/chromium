// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_UTIL_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_UTIL_H_

#include "base/time/time.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "ui/gfx/vector_icon_types.h"

namespace content_settings {

class CookieControlsUtil {
 public:
  // A return value of:
  //   0  represents expiration today
  //   1  represents expiration tomorrow
  //   2  represents expiration in 2 days
  //  -1  represents expiration yesterday
  static int GetDaysToExpiration(base::Time expiration);

  static const gfx::VectorIcon& GetEnforcedIcon(
      CookieControlsEnforcement enforcement);

  static std::u16string GetEnforcedTooltip(
      CookieControlsEnforcement enforcement);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_UTIL_H_
