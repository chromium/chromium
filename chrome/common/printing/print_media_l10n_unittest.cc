// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is only built and run on platforms allowing print media
// localization.

#include <string>
#include <vector>

#include "chrome/common/printing/print_media_l10n.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

// Verifies that we localize some common names.
TEST(PrintMediaL10N, LocalizeSomeCommonNames) {
  const struct {
    const char* vendor_id;
    const char* expected_localized_name;
  } kTestCases[] = {
      {"na_c_17x22in", "17 x 22 in"},
      {"iso_a0_841x1189mm", "A0"},
      {"iso_a1_594x841mm", "A1"},
      {"iso_a4_210x297mm", "A4"},
      {"oe_photo-l_3.5x5in", "3.5 x 5 in"},
      {"om_business-card_55x91mm", "55 x 91 mm"},
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
      {"", ""},
      {"lorem_ipsum_8x10", ""},
      {"q_e_d_130x200mm", ""},
      {"not at all a valid vendor ID", ""},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(LocalizePaperDisplayName(test_case.vendor_id),
              test_case.expected_localized_name);
  }
}

// Verifies that duplicates have the same localization.
TEST(PrintMediaL10N, LocalizeDuplicateNames) {
  const struct {
    const char* duplicate_vendor_id;
    const char* vendor_id;
  } kTestCases[] = {
      {"oe_photo-s10r_10x15in", "na_10x15_10x15in"},
      {"om_large-photo_200x300", "om_large-photo_200x300mm"},
      {"om_postfix_114x229mm", "iso_c6c5_114x229mm"},
      {"prc_10_324x458mm", "iso_c3_324x458mm"},
      {"prc_3_125x176mm", "iso_b6_125x176mm"},
      {"prc_5_110x220mm", "iso_dl_110x220mm"},
      {"iso_id-3_88x125mm", "iso_b7_88x125mm"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(LocalizePaperDisplayName(test_case.duplicate_vendor_id),
              LocalizePaperDisplayName(test_case.vendor_id));
  }
}

// Verifies that we generate names for unrecognized sizes correctly.
TEST(PrintMediaL10N, LocalizeSelfDescribingSizes) {
  const struct {
    const char* vendor_id;
    const char* expected_localized_name;
  } kTestCases[] = {
      {"invalid_size", ""},
      {"om_photo-31x41_310x410mm", "310 x 410 mm"},
      {"om_t-4-x-7_4x7in", "4 x 7 in"},
      {"om_4-x-7_101.6x180.6mm", "4 X 7 (101.6 x 180.6 mm)"},
      {"om_custom-1_209.9x297.04mm", "Custom 1 (209.9 x 297.04 mm)"},
      {"om_double-postcard-rotated_200.03x148.17mm",
       "Double Postcard Rotated (200.03 x 148.17 mm)"},
      {"oe_photo-8x10-tab_8x10.5in", "Photo 8x10 Tab (8 x 10.5 in)"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(LocalizePaperDisplayName(test_case.vendor_id),
              test_case.expected_localized_name);
  }
}

}  // namespace printing
