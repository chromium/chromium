// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/sidebar_promos.h"

#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr int kExpiringId = 1;
constexpr int kPermanentId = 2;
constexpr base::Time::Exploded kExpiry{.year = 2027,
                                       .month = 1,
                                       .day_of_month = 1};

constexpr SidebarPromo kPromos[] = {
    {kExpiringId, "https://example.com/expiring", kExpiry},
    {kPermanentId, "https://example.com/permanent", std::nullopt},
};

base::Time Expiry() {
  base::Time time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kExpiry, &time));
  return time;
}

int PickedId(base::span<const SidebarPromo> promos, base::Time when) {
  std::optional<SidebarPromo> promo = PickSidebarPromo(promos, when);
  return promo ? promo->message_id : 0;
}

}  // namespace

TEST(SidebarPromosTest, RotatesDaily) {
  const base::Time day_one = Expiry() - base::Days(60);
  const int first = PickedId(kPromos, day_one);
  const int second = PickedId(kPromos, day_one + base::Days(1));
  EXPECT_NE(first, second);
  EXPECT_TRUE(first == kExpiringId || first == kPermanentId);
  EXPECT_TRUE(second == kExpiringId || second == kPermanentId);

  // The same promo is shown for the whole day.
  EXPECT_EQ(first, PickedId(kPromos, day_one + base::Hours(23)));
  // And comes back around once every promo has had a day.
  EXPECT_EQ(first, PickedId(kPromos, day_one + base::Days(2)));
}

TEST(SidebarPromosTest, SkipsExpiredPromos) {
  EXPECT_NE(PickedId(kPromos, Expiry() - base::Days(2)),
            PickedId(kPromos, Expiry() - base::Days(1)));

  EXPECT_EQ(kPermanentId, PickedId(kPromos, Expiry()));
  EXPECT_EQ(kPermanentId, PickedId(kPromos, Expiry() + base::Days(1)));
  EXPECT_EQ(kPermanentId, PickedId(kPromos, Expiry() + base::Days(1000)));
}

TEST(SidebarPromosTest, NothingToShow) {
  EXPECT_EQ(std::nullopt, PickSidebarPromo({}, Expiry() - base::Days(60)));

  const base::span<const SidebarPromo> all_expired =
      base::span(kPromos).first(1u);
  EXPECT_EQ(kExpiringId, PickedId(all_expired, Expiry() - base::Seconds(1)));
  EXPECT_EQ(std::nullopt, PickSidebarPromo(all_expired, Expiry()));
}

}  // namespace extensions
