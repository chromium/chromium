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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace content_settings {
namespace {

// Get the local time, round down to midnight on the current day, and then load
// this time as a base::Time UTC time.  This is unusual, but when we find the
// TimeDelta::InDays() within GetDaysToExpiration() we want to make sure we're
// counting actual days in the local timezone, not actual days in UTC time.
base::Time LocalMidnightAsUTCTime(base::Time t) {
  base::Time::Exploded exploded;
  t.LocalExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time out;
  bool result = base::Time::FromUTCExploded(exploded, &out);
  DCHECK(result);
  return out;
}

}  // namespace

// static
int CookieControlsUtil::GetDaysToExpiration(base::Time expiration) {
  const base::Time midnight_today = LocalMidnightAsUTCTime(base::Time::Now());
  const base::Time midnight_expiration = LocalMidnightAsUTCTime(expiration);
  return (midnight_expiration - midnight_today).InDays();
}

// static
const gfx::VectorIcon& CookieControlsUtil::GetEnforcedIcon(
    CookieControlsEnforcement enforcement) {
  switch (enforcement) {
    case CookieControlsEnforcement::kEnforcedByExtension:
      return vector_icons::kExtensionChromeRefreshIcon;
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return vector_icons::kBusinessChromeRefreshIcon;
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return vector_icons::kSettingsChromeRefreshIcon;
    case CookieControlsEnforcement::kEnforcedByTpcdGrant:
    case CookieControlsEnforcement::kNoEnforcement:
      NOTREACHED();
  }
}

// static
std::u16string CookieControlsUtil::GetEnforcedTooltip(
    CookieControlsEnforcement enforcement) {
  switch (enforcement) {
    case CookieControlsEnforcement::kEnforcedByExtension:
      return l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION);
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY);
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_MANAGED_BY_SETTINGS_TOOLTIP);
    case CookieControlsEnforcement::kEnforcedByTpcdGrant:
    case CookieControlsEnforcement::kNoEnforcement:
      NOTREACHED();
  }
}

}  // namespace content_settings
