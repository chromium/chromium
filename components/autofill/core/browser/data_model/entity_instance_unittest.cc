// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_instance.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

const EntityInstance& passport() {
  using enum AttributeTypeName;
  static const EntityInstance kMyPassport(
      EntityType(EntityTypeName::kPassport),
      {AttributeInstance(AttributeType(kPassportName), "Donald Duck", {}),
       AttributeInstance(AttributeType(kPassportCountry), "USA", {}),
       AttributeInstance(AttributeType(kPassportExpiryDate), "09/2098", {}),
       AttributeInstance(AttributeType(kPassportIssueDate), "10/1998", {})},
      base::Uuid::GenerateRandomV4(), "Passie", base::TimeTicks::Now(),
      /*synced=*/false);
  return kMyPassport;
}

const EntityInstance& loyalty_card() {
  using enum AttributeTypeName;
  static const EntityInstance kMyLoyaltyCard(
      EntityType(EntityTypeName::kLoyaltyCard),
      {AttributeInstance(AttributeType(kLoyaltyCardProgram),
                         "Asterisk Alliance", {}),
       AttributeInstance(AttributeType(kLoyaltyCardProvider), "Duck Airways",
                         {}),
       AttributeInstance(AttributeType(kLoyaltyCardMemberId), "1", {})},
      base::Uuid::GenerateRandomV4(), "Duckie", base::TimeTicks::Now(),
      /*synced=*/true);
  return kMyLoyaltyCard;
}

TEST(AutofillEntityInstanceTest, Attributes) {
  using enum AttributeTypeName;
  EXPECT_EQ(passport().attributes().size(), 4u);
  EXPECT_EQ(passport().type().attributes().size(), 6u);
  EXPECT_FALSE(passport().attribute(AttributeType(kPassportNumber)));
  {
    base::optional_ref<const AttributeInstance> a =
        passport().attribute(AttributeType(kPassportName));
    ASSERT_TRUE(a);
    EXPECT_THAT(a->type(), AttributeType(kPassportName));
    EXPECT_THAT(a->value(), "Donald Duck");
  }
}

TEST(AutofillEntityInstanceTest, Synced) {
  EXPECT_FALSE(passport().synced());
  EXPECT_TRUE(loyalty_card().synced());
}

}  // namespace
}  // namespace autofill
