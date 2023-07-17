// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_util.h"

#include <memory>
#include <string>
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"

namespace content_settings {

// static
int CookieControlsUtil::GetDaysToExpiration(base::Time expiration) {
  // TODO(crbug.com/1446230): Apply DST corrections.
  const base::Time midnight_today = base::Time::Now().LocalMidnight();
  const base::Time midnight_expiration = expiration.LocalMidnight();
  return (midnight_expiration - midnight_today).InDays();
}

// static
const gfx::VectorIcon& CookieControlsUtil::GetEnforcedIcon(
    CookieControlsEnforcement enforcement) {
  switch (enforcement) {
    case CookieControlsEnforcement::kEnforcedByExtension:
      return vector_icons::kExtensionIcon;
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return vector_icons::kBusinessIcon;
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return vector_icons::kSettingsIcon;
    case CookieControlsEnforcement::kNoEnforcement:
      NOTREACHED_NORETURN();
  }
}

// static
int CookieControlsUtil::GetEnforcedTooltipTextId(
    CookieControlsEnforcement enforcement) {
  switch (enforcement) {
    case CookieControlsEnforcement::kEnforcedByExtension:
      return IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION;
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY;
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_MANAGED_BY_SETTINGS_TOOLTIP;
    case CookieControlsEnforcement::kNoEnforcement:
      NOTREACHED_NORETURN();
  }
}

}  // namespace content_settings
