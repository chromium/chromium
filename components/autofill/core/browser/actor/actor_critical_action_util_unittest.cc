// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/actor/actor_critical_action_util.h"

#include <optional>

#include "components/autofill/core/browser/field_types.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using critical_actions::FormFillType;

TEST(ActorCriticalActionUtilTest, FieldTypeGroups) {
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kAddress), FormFillType::kAddress);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kCompany), FormFillType::kAddress);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kName), FormFillType::kName);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kEmail), FormFillType::kEmail);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kPhone), FormFillType::kPhone);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kCreditCard),
            FormFillType::kCreditCard);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kStandaloneCvcField),
            FormFillType::kCreditCard);
  // Not supported by the agent yet.
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kIban), std::nullopt);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kLoyaltyCard), std::nullopt);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kAutofillAi), std::nullopt);
  // Covered by other action types or not user data.
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kPasswordField), std::nullopt);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kOneTimePassword), std::nullopt);
  EXPECT_EQ(ToFormFillType(FieldTypeGroup::kNoGroup), std::nullopt);
}

}  // namespace
}  // namespace autofill
