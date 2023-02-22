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

using Paper = PrinterSemanticCapsAndDefaults::Paper;

namespace {

struct MediaInfoTestCase {
  const char* vendor_id;
  std::u16string expected_localized_name;
  MediaSizeGroup expected_group;
};

void VerifyLocalizedInfo(const MediaInfoTestCase& test_case) {
  MediaSizeInfo info = LocalizePaperDisplayName(test_case.vendor_id);
  EXPECT_EQ(info.name, test_case.expected_localized_name);
  EXPECT_EQ(info.sort_group, test_case.expected_group);
}

void VerifyPaperSizeMatch(const PaperWithSizeInfo& lhs,
                          const PaperWithSizeInfo& rhs) {
  EXPECT_EQ(lhs.size_info.name, rhs.size_info.name);
  EXPECT_EQ(lhs.size_info.sort_group, rhs.size_info.sort_group);
  EXPECT_EQ(lhs.paper, rhs.paper);
}

}  // namespace

// Verifies that we localize some common names.
TEST(PrintMediaL10N, LocalizeSomeCommonNames) {
  const MediaInfoTestCase kTestCases[] = {
      {"na_c_17x22in", u"17 x 22 in", MediaSizeGroup::kSizeIn},
      {"iso_a0_841x1189mm", u"A0", MediaSizeGroup::kSizeNamed},
      {"iso_a1_594x841mm", u"A1", MediaSizeGroup::kSizeNamed},
      {"iso_a4_210x297mm", u"A4", MediaSizeGroup::kSizeNamed},
      {"na_letter_8.5x11in", u"Letter", MediaSizeGroup::kSizeNamed},
      {"oe_photo-l_3.5x5in", u"3.5 x 5 in", MediaSizeGroup::kSizeIn},
      {"om_business-card_55x91mm", u"55 x 91 mm", MediaSizeGroup::kSizeMm},
  };

  for (const auto& test_case : kTestCases) {
    VerifyLocalizedInfo(test_case);
  }
}

// Verifies that we return the empty string when no localization is
// found for a given media name.
TEST(PrintMediaL10N, DoWithoutCommonName) {
  const MediaInfoTestCase kTestCases[] = {
      {"", u"", MediaSizeGroup::kSizeNamed},
      {"lorem_ipsum_8x10", u"", MediaSizeGroup::kSizeNamed},
      {"q_e_d_130x200mm", u"", MediaSizeGroup::kSizeNamed},
      {"not at all a valid vendor ID", u"", MediaSizeGroup::kSizeNamed},
  };

  for (const auto& test_case : kTestCases) {
    VerifyLocalizedInfo(test_case);
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
      {"na_letter_8.5x11in", "na_card-letter_8.5x11in"},
      {"na_letter_8.5x11in", "na_letter.fb_8.5x11in"},
      {"na_letter_8.5x11in", "na_card-letter.fb_8.5x11in"},
  };

  for (const auto& test_case : kTestCases) {
    MediaSizeInfo duplicate =
        LocalizePaperDisplayName(test_case.duplicate_vendor_id);
    MediaSizeInfo original = LocalizePaperDisplayName(test_case.vendor_id);

    EXPECT_EQ(duplicate.name, original.name);
    EXPECT_EQ(duplicate.sort_group, original.sort_group);
  }
}

// Verifies that we generate names for unrecognized sizes correctly.
TEST(PrintMediaL10N, LocalizeSelfDescribingSizes) {
  const MediaInfoTestCase kTestCases[] = {
      {"invalid_size", u"", MediaSizeGroup::kSizeNamed},
      {"om_photo-31x41_310x410mm", u"310 x 410 mm", MediaSizeGroup::kSizeMm},
      {"om_t-4-x-7_4x7in", u"4 x 7 in", MediaSizeGroup::kSizeIn},
      {"om_4-x-7_101.6x180.6mm", u"4 X 7 (101.6 x 180.6 mm)",
       MediaSizeGroup::kSizeNamed},
      {"om_custom-1_209.9x297.04mm", u"Custom 1 (209.9 x 297.04 mm)",
       MediaSizeGroup::kSizeNamed},
      {"om_double-postcard-rotated_200.03x148.17mm",
       u"Double Postcard Rotated (200.03 x 148.17 mm)",
       MediaSizeGroup::kSizeNamed},
      {"oe_photo-8x10-tab_8x10.5in", u"Photo 8x10 Tab (8 x 10.5 in)",
       MediaSizeGroup::kSizeNamed},
      {"na_card-letter_8.5x11in", u"Letter", MediaSizeGroup::kSizeNamed},
      {"na_letter.fb_8.5x11in", u"Letter", MediaSizeGroup::kSizeNamed},
  };

  for (const auto& test_case : kTestCases) {
    VerifyLocalizedInfo(test_case);
  }
}

// Verifies that paper sizes are returned in the expected order of groups.
TEST(PrintMediaL10N, SortGroupsOrdered) {
  PaperWithSizeInfo mm = {
      MediaSizeInfo{u"mm", MediaSizeGroup::kSizeMm, /*registered_size=*/false},
      Paper{"metric", "mm", gfx::Size()}};
  PaperWithSizeInfo in = {
      MediaSizeInfo{u"in", MediaSizeGroup::kSizeIn, /*registered_size=*/false},
      Paper{"inches", "in", gfx::Size()}};
  PaperWithSizeInfo named = {MediaSizeInfo{u"named", MediaSizeGroup::kSizeNamed,
                                           /*registered_size=*/false},
                             Paper{"named size", "named", gfx::Size()}};

  std::vector<PaperWithSizeInfo> papers = {mm, named, in};
  std::vector<PaperWithSizeInfo> expected = {in, mm, named};
  SortPaperDisplayNames(papers);
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

// Verifies that inch paper sizes are sorted by width, height, name.
TEST(PrintMediaL10N, SortInchSizes) {
  PaperWithSizeInfo p1 = {
      MediaSizeInfo{u"1x3", MediaSizeGroup::kSizeIn, /*registered_size=*/false},
      Paper{"1x3", "in", gfx::Size(1, 3)}};
  PaperWithSizeInfo p2 = {
      MediaSizeInfo{u"2x1", MediaSizeGroup::kSizeIn, /*registered_size=*/false},
      Paper{"2x1", "in", gfx::Size(2, 1)}};
  PaperWithSizeInfo p3 = {
      MediaSizeInfo{u"2x2", MediaSizeGroup::kSizeIn, /*registered_size=*/false},
      Paper{"2x2", "in", gfx::Size(2, 2)}};
  PaperWithSizeInfo p4 = {MediaSizeInfo{u"2x2 B", MediaSizeGroup::kSizeIn,
                                        /*registered_size=*/false},
                          Paper{"2x2 B", "in", gfx::Size(2, 2)}};

  std::vector<PaperWithSizeInfo> papers = {p4, p1, p2, p3};
  std::vector<PaperWithSizeInfo> expected = {p1, p2, p3, p4};
  SortPaperDisplayNames(papers);
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

// Verifies that mm paper sizes are sorted by width, height, name.
TEST(PrintMediaL10N, SortMmSizes) {
  PaperWithSizeInfo p1 = {
      MediaSizeInfo{u"1x3", MediaSizeGroup::kSizeMm, /*registered_size=*/false},
      Paper{"1x3", "mm", gfx::Size(1, 3)}};
  PaperWithSizeInfo p2 = {
      MediaSizeInfo{u"2x1", MediaSizeGroup::kSizeMm, /*registered_size=*/false},
      Paper{"2x1", "mm", gfx::Size(2, 1)}};
  PaperWithSizeInfo p3 = {
      MediaSizeInfo{u"2x2", MediaSizeGroup::kSizeMm, /*registered_size=*/false},
      Paper{"2x2", "mm", gfx::Size(2, 2)}};
  PaperWithSizeInfo p4 = {MediaSizeInfo{u"2x2 B", MediaSizeGroup::kSizeMm,
                                        /*registered_size=*/false},
                          Paper{"2x2 B", "mm", gfx::Size(2, 2)}};

  std::vector<PaperWithSizeInfo> papers = {p4, p1, p2, p3};
  std::vector<PaperWithSizeInfo> expected = {p1, p2, p3, p4};
  SortPaperDisplayNames(papers);
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

// Verifies that named paper sizes are sorted by name, width, height.
TEST(PrintMediaL10N, SortNamedSizes) {
  PaperWithSizeInfo p1 = {MediaSizeInfo{u"AAA", MediaSizeGroup::kSizeNamed,
                                        /*registered_size=*/false},
                          Paper{"AAA", "name", gfx::Size(50, 50)}};
  PaperWithSizeInfo p2 = {MediaSizeInfo{u"BBB", MediaSizeGroup::kSizeNamed,
                                        /*registered_size=*/false},
                          Paper{"BBB", "name1", gfx::Size(1, 3)}};
  PaperWithSizeInfo p3 = {MediaSizeInfo{u"BBB", MediaSizeGroup::kSizeNamed,
                                        /*registered_size=*/false},
                          Paper{"BBB", "name2", gfx::Size(2, 2)}};
  PaperWithSizeInfo p4 = {MediaSizeInfo{u"BBB", MediaSizeGroup::kSizeNamed,
                                        /*registered_size=*/false},
                          Paper{"BBB", "name3", gfx::Size(2, 3)}};

  std::vector<PaperWithSizeInfo> papers = {p4, p1, p2, p3};
  std::vector<PaperWithSizeInfo> expected = {p1, p2, p3, p4};
  SortPaperDisplayNames(papers);
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

TEST(PrintMediaL10N, RemoveBorderlessSizes) {
  PaperWithSizeInfo p1 = {MediaSizeInfo{u"AAA", MediaSizeGroup::kSizeNamed,
                                        /*registered_size=*/false},
                          Paper{"AAA", "oe_aaa.fb_8x10in", gfx::Size(8, 10)}};
  PaperWithSizeInfo p2 = {MediaSizeInfo{u"BBB", MediaSizeGroup::kSizeNamed},
                          Paper{"BBB", "oe_bbb_4x6in", gfx::Size(4, 6)}};
  PaperWithSizeInfo p3 = {
      MediaSizeInfo{u"BBB", MediaSizeGroup::kSizeNamed},
      Paper{"BBB", "oe_bbb.borderless_4x6in", gfx::Size(4, 6)}};
  PaperWithSizeInfo p4 = {MediaSizeInfo{u"AAA", MediaSizeGroup::kSizeNamed,
                                        /*registered_size=*/false},
                          Paper{"AAA", "oe_aaa.8x10in", gfx::Size(8, 10)}};

  std::vector<PaperWithSizeInfo> papers = {p1, p2, p3, p4};
  std::vector<PaperWithSizeInfo> expected = {p4, p2};
  SortPaperDisplayNames(papers);
  ASSERT_EQ(papers.size(), expected.size());
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

// Verifies that PWG registered size names sort above unregistered names with
// the same dimensions.
TEST(PrintMediaL10N, SortNonstandardSizes) {
  PaperWithSizeInfo p1 = {
      MediaSizeInfo{u"AAA", MediaSizeGroup::kSizeNamed,
                    /*registered_size=*/false},
      Paper{"AAA", "na_card-letter_8.5x11in", gfx::Size(9, 11)}};
  PaperWithSizeInfo p2 = {
      MediaSizeInfo{u"AAA", MediaSizeGroup::kSizeNamed,
                    /*registered_size=*/false},
      Paper{"AAA", "na_card-letter_8x9in", gfx::Size(8, 11)}};
  PaperWithSizeInfo p3 = {
      MediaSizeInfo{u"AAA", MediaSizeGroup::kSizeIn, /*registered_size=*/false},
      Paper{"AAA", "oe_aaa.8x10in", gfx::Size(8, 10)}};
  PaperWithSizeInfo p4 = {MediaSizeInfo{u"AAA", MediaSizeGroup::kSizeNamed,
                                        /*registered_size=*/true},
                          Paper{"AAA", "na_letter_8.5x11in", gfx::Size(9, 11)}};
  PaperWithSizeInfo p5 = {
      MediaSizeInfo{u"BBB", MediaSizeGroup::kSizeIn, /*registered_size=*/true},
      Paper{"BBB", "na_govt-letter_8x10in", gfx::Size(8, 10)}};
  PaperWithSizeInfo p6 = {
      MediaSizeInfo{u"AAA", MediaSizeGroup::kSizeIn, /*registered_size=*/true},
      Paper{"AAA", "na_govt-letter_8x10in", gfx::Size(8, 10)}};

  std::vector<PaperWithSizeInfo> papers = {p1, p2, p3, p4, p5, p6};
  std::vector<PaperWithSizeInfo> expected = {p6, p5, p3, p4, p2, p1};
  SortPaperDisplayNames(papers);
  ASSERT_EQ(papers.size(), expected.size());
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

}  // namespace printing
