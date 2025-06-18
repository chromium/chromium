// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education {

namespace {

const NtpPromoIdentifier kPromoId = "test_promo";
const NtpPromoIdentifier kShowFirstPromoId = "show_first_promo";
constexpr char kIconName[] = "test_icon_name";
constexpr int kBodyTextStringId = 123;
constexpr int kActionButtonTextStringId = 456;

// Helper to create a basic NtpPromoSpecification for testing.
NtpPromoSpecification CreateTestPromoSpec(const NtpPromoIdentifier& id) {
  return NtpPromoSpecification(
      id,
      NtpPromoContent(kIconName, kBodyTextStringId, kActionButtonTextStringId),
      base::BindRepeating([](Profile* profile) {
        return NtpPromoSpecification::Eligibility::kEligible;
      }),
      base::BindRepeating([](Browser* browser) {}),
      base::flat_set<NtpPromoIdentifier>{kShowFirstPromoId},
      user_education::Metadata());
}

}  // namespace

class NtpPromoRegistryTest : public testing::Test {
 public:
  NtpPromoRegistryTest() = default;
  ~NtpPromoRegistryTest() override = default;

 protected:
  NtpPromoRegistry registry_;
};

TEST_F(NtpPromoRegistryTest, RegisterPromo) {
  registry_.AddPromo(NtpPromoSpecification(
      kPromoId,
      NtpPromoContent(kIconName, kBodyTextStringId, kActionButtonTextStringId),
      base::BindRepeating([](Profile* profile) {
        return NtpPromoSpecification::Eligibility::kEligible;
      }),
      base::BindRepeating([](Browser* browser) {}), {kShowFirstPromoId},
      user_education::Metadata()));

  const auto* spec = registry_.GetNtpPromoSpecification(kPromoId);
  ASSERT_NE(spec, nullptr);
  EXPECT_EQ(spec->content().icon_name(), kIconName);
  EXPECT_EQ(spec->content().body_text_string_id(), kBodyTextStringId);
  EXPECT_EQ(spec->content().action_button_text_string_id(),
            kActionButtonTextStringId);
  EXPECT_THAT(spec->show_after(), testing::ElementsAre(kShowFirstPromoId));
}

TEST_F(NtpPromoRegistryTest, GetIdentifiers) {
  registry_.AddPromo(CreateTestPromoSpec("Promo1"));
  registry_.AddPromo(CreateTestPromoSpec("Promo2"));
  EXPECT_THAT(registry_.GetNtpPromoIdentifiers(),
              testing::ElementsAre("Promo1", "Promo2"));
}

TEST_F(NtpPromoRegistryTest, DuplicateEntry) {
  registry_.AddPromo(CreateTestPromoSpec("Promo1"));
  EXPECT_CHECK_DEATH(registry_.AddPromo(CreateTestPromoSpec("Promo1")));
}

}  // namespace user_education
