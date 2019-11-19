// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is only built and run on platforms allowing print media
// localization.

#include <string>
#include <vector>

#include "components/printing/browser/print_media_l10n.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

// Verifies that we localize some common names.
TEST(PrintMediaL10N, LocalizeSomeCommonNames) {
  const struct {
    const char* vendor_id;
    const char* expected_localized_name;
  } kTestCases[] = {
      {"na_c_17x22in", "Engineering-C"},
      {"iso_a0_841x1189mm", "A0"},
      {"iso_a1_594x841mm", "A1"},
      {"iso_a4_210x297mm", "A4"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(LocalizePaperDisplayName(test_case.vendor_id),
              test_case.expected_localized_name);
  }
}

// Verifies that we return the empty string when no localization is
// found for a given media name.
TEST(PrintMediaL10N, DoWithoutCommonName) {
  const struct {
    const char* vendor_id;
    const char* expected_localized_name;
  } kTestCases[] = {
      {"lorem_ipsum_8x10in", ""},
      {"q_e_d_130x200mm", ""},
      {"not at all a valid vendor ID", ""},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(LocalizePaperDisplayName(test_case.vendor_id),
              test_case.expected_localized_name);
  }
}

}  // namespace printing
