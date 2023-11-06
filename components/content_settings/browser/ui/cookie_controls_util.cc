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
      return vector_icons::kExtensionIcon;
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return vector_icons::kBusinessIcon;
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return vector_icons::kSettingsIcon;
    case CookieControlsEnforcement::kEnforcedByTpcdGrant:
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
    case CookieControlsEnforcement::kEnforcedByTpcdGrant:
    case CookieControlsEnforcement::kNoEnforcement:
      NOTREACHED_NORETURN();
  }
}

}  // namespace content_settings
