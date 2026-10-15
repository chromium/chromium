// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/sidebar_promos.h"

#include <stddef.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/grit/generated_resources.h"

namespace extensions {

namespace {

// To add a promo, add its message to chrome/app/extensions_strings.grdp, with
// a translation screenshot, and add a row here.
constexpr SidebarPromo kSidebarPromos[] = {
    {IDS_EXTENSIONS_MODERN_WEB_GUIDANCE_PROMO,
     "https://developer.chrome.com/docs/extensions/ai/"
     "build-with-ai#modern_web_guidance",
     base::Time::Exploded{.year = 2027, .month = 1, .day_of_month = 1}},
    {IDS_EXTENSIONS_WHATS_NEW_PROMO,
     "https://developer.chrome.com/docs/extensions/whats-new", std::nullopt},
};

constexpr bool IsWellFormed(const SidebarPromo& promo) {
  if (promo.message_id == 0 ||
      !std::string_view(promo.url).starts_with("https://")) {
    return false;
  }
  if (!promo.expiration) {
    return true;
  }
  return 1 <= promo.expiration->month && promo.expiration->month <= 12 &&
         1 <= promo.expiration->day_of_month &&
         promo.expiration->day_of_month <= 31;
}
static_assert(std::ranges::all_of(kSidebarPromos, IsWellFormed));

bool HasExpired(const SidebarPromo& promo, base::Time now) {
  if (!promo.expiration) {
    return false;
  }
  base::Time expiration;
  CHECK(base::Time::FromUTCExploded(*promo.expiration, &expiration));
  return now >= expiration;
}

}  // namespace

base::span<const SidebarPromo> GetSidebarPromos() {
  return kSidebarPromos;
}

std::optional<SidebarPromo> PickSidebarPromo(
    base::span<const SidebarPromo> promos,
    base::Time now) {
  std::vector<SidebarPromo> unexpired;
  for (const SidebarPromo& promo : promos) {
    if (!HasExpired(promo, now)) {
      unexpired.push_back(promo);
    }
  }
  if (unexpired.empty()) {
    return std::nullopt;
  }
  const size_t day =
      base::checked_cast<size_t>((now - base::Time::UnixEpoch()).InDays());
  return unexpired[day % unexpired.size()];
}

}  // namespace extensions
