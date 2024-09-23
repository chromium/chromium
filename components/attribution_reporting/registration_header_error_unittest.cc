// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_header_error.h"

#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/os_registration_error.mojom-shared.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

TEST(RegistrationHeaderErrorTest, HeaderName) {
  const struct {
    RegistrationHeaderErrorDetails error;
    const char* expected;
  } kTestCases[] = {
      {
          mojom::SourceRegistrationError::kInvalidJson,
          kAttributionReportingRegisterSourceHeader,
      },
      {
          mojom::TriggerRegistrationError::kInvalidJson,
          kAttributionReportingRegisterTriggerHeader,
      },
      {
          OsSourceRegistrationError(mojom::OsRegistrationError::kInvalidList),
          kAttributionReportingRegisterOsSourceHeader,
      },
      {
          OsTriggerRegistrationError(mojom::OsRegistrationError::kInvalidList),
          kAttributionReportingRegisterOsTriggerHeader,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.expected);
    RegistrationHeaderError error(/*header_value=*/"", test_case.error);
    EXPECT_EQ(error.HeaderName(), test_case.expected);
  }
}

}  // namespace
}  // namespace attribution_reporting
