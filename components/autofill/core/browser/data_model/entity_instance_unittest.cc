// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_instance.h"

#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

TEST(AutofillEntityInstanceTest, Attributes) {
  const char kName[] = "Pippi";
  EntityInstance pp =
      test::GetPassportEntityInstance({.name = kName, .number = nullptr});
  using enum AttributeTypeName;
  EXPECT_EQ(pp.attributes().size(), 4u);
  EXPECT_EQ(pp.type().attributes().size(), 5u);
  EXPECT_FALSE(pp.attribute(AttributeType(kPassportNumber)));
  {
    base::optional_ref<const AttributeInstance> a =
        pp.attribute(AttributeType(kPassportName));
    ASSERT_TRUE(a);
    EXPECT_THAT(a->type(), AttributeType(kPassportName));
    EXPECT_THAT(a->value(), std::string_view(kName));
  }
}

}  // namespace
}  // namespace autofill
