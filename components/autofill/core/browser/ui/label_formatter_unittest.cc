// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/label_formatter.h"

#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

TEST(LabelFormatterTest, CreateWithMissingFieldTypes) {
  const std::vector<AutofillProfile*> profiles{};
  EXPECT_EQ(LabelFormatter::Create(profiles, "en-US", NAME_FIRST,
                                   std::vector<ServerFieldType>()),
            nullptr);
}

TEST(LabelFormatterTest, CreateWithUnsupportedFieldTypes) {
  const std::vector<AutofillProfile*> profiles{};
  EXPECT_EQ(
      LabelFormatter::Create(profiles, "en-US", USERNAME, {USERNAME, PASSWORD}),
      nullptr);
}

}  // namespace
}  // namespace autofill