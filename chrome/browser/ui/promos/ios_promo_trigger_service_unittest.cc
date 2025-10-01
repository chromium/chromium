// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promo_trigger_service.h"

#include <optional>

#include "base/callback_list.h"
#include "chrome/browser/promos/promos_types.h"
#include "testing/gtest/include/gtest/gtest.h"

class IOSPromoTriggerServiceTest : public testing::Test {
 public:
  IOSPromoTriggerServiceTest() = default;
  ~IOSPromoTriggerServiceTest() override = default;

 protected:
  IOSPromoTriggerService service_;
};

TEST_F(IOSPromoTriggerServiceTest, NotifiesCallback) {
  int call_count = 0;
  std::optional<IOSPromoType> promo_type;

  base::CallbackListSubscription subscription =
      service_.RegisterPromoCallback(base::BindRepeating(
          [](int* call_count, std::optional<IOSPromoType>* promo_type,
             IOSPromoType type) {
            (*call_count)++;
            *promo_type = type;
          },
          &call_count, &promo_type));

  EXPECT_EQ(call_count, 0);
  EXPECT_FALSE(promo_type.has_value());

  service_.NotifyPromoShouldBeShown(IOSPromoType::kPassword);

  EXPECT_EQ(call_count, 1);
  ASSERT_TRUE(promo_type.has_value());
  EXPECT_EQ(promo_type.value(), IOSPromoType::kPassword);
}

TEST_F(IOSPromoTriggerServiceTest,
       CallbackIsRemovedWhenSubscriptionIsDestroyed) {
  int call_count = 0;
  std::optional<IOSPromoType> promo_type;

  {
    base::CallbackListSubscription subscription =
        service_.RegisterPromoCallback(base::BindRepeating(
            [](int* call_count, std::optional<IOSPromoType>* promo_type,
               IOSPromoType type) {
              (*call_count)++;
              *promo_type = type;
            },
            &call_count, &promo_type));
  }

  service_.NotifyPromoShouldBeShown(IOSPromoType::kPassword);

  EXPECT_EQ(call_count, 0);
}
