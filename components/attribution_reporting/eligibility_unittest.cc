// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/eligibility.h"

#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/eligibility_error.mojom-shared.h"
#include "components/attribution_reporting/registration_type.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::EligibilityError;
using ::attribution_reporting::mojom::RegistrationType;

TEST(EligibilityTest, ParseEligibleHeader) {
  const struct {
    absl::optional<base::StringPiece> header;
    base::expected<RegistrationType, EligibilityError> expected;
  } kTestCases[] = {
      {
          absl::nullopt,
          RegistrationType::kTrigger,
      },
      {
          "trigger",
          RegistrationType::kTrigger,
      },
      {
          "event-source",
          RegistrationType::kSource,
      },
      {
          "trigger, event-source",
          RegistrationType::kSourceOrTrigger,
      },
      {
          "!",
          base::unexpected(EligibilityError::kInvalidStructuredHeader),
      },
      {
          "navigation-source",
          base::unexpected(EligibilityError::kContainsNavigationSource),
      },
      {
          "",
          base::unexpected(EligibilityError::kIneligible),
      },
      {
          "event-source=2",  // value ignored
          RegistrationType::kSource,
      },
      {
          "event-source;a=3",  // parameter ignored
          RegistrationType::kSource,
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(ParseEligibleHeader(test_case.header), test_case.expected)
        << (test_case.header ? *test_case.header : "(null)");
  }
}

}  // namespace
}  // namespace attribution_reporting
