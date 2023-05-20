// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is only built and run on platforms allowing print media
// localization.

#include <string>
#include <vector>

#include "chrome/common/printing/print_media_l10n.h"
#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

using Paper = PrinterSemanticCapsAndDefaults::Paper;

namespace {

struct MediaInfoTestCase {
  gfx::Size size_um;
  std::string expected_vendor_id;
  std::u16string expected_localized_name;
  MediaSizeGroup expected_group;
};

void VerifyLocalizedInfo(const MediaInfoTestCase& test_case) {
  MediaSizeInfo info = LocalizePaperDisplayName(test_case.size_um);
  EXPECT_EQ(info.vendor_id, test_case.expected_vendor_id);
  EXPECT_EQ(info.display_name, test_case.expected_localized_name);
  EXPECT_EQ(info.sort_group, test_case.expected_group);
}

void VerifyPaperSizeMatch(const PaperWithSizeInfo& lhs,
                          const PaperWithSizeInfo& rhs) {
  EXPECT_EQ(lhs.size_info.vendor_id, rhs.size_info.vendor_id);
  EXPECT_EQ(lhs.size_info.display_name, rhs.size_info.display_name);
  EXPECT_EQ(lhs.size_info.sort_group, rhs.size_info.sort_group);
  EXPECT_EQ(lhs.paper, rhs.paper);
}

}  // namespace

// Verifies that we localize some common paper sizes.
TEST(PrintMediaL10N, LocalizeSomeCommonSizes) {
  const MediaInfoTestCase kTestCases[] = {
      {{431800, 558800},
       "na_c_17x22in",
       u"17 x 22 in",
       MediaSizeGroup::kSizeIn},
      {{841000, 1189000},
       "iso_a0_841x1189mm",
       u"A0",
       MediaSizeGroup::kSizeNamed},
      {{594000, 841000}, "iso_a1_594x841mm", u"A1", MediaSizeGroup::kSizeNamed},
      {{210000, 297000}, "iso_a4_210x297mm", u"A4", MediaSizeGroup::kSizeNamed},
      {{203200, 330200},
       "na_govt-legal_8x13in",
       u"8 x 13 in",
       MediaSizeGroup::kSizeIn},
      {{203200, 254000},
       "na_govt-letter_8x10in",
       u"8 x 10 in",
       MediaSizeGroup::kSizeIn},
      {{215900, 279400},
       "na_letter_8.5x11in",
       u"Letter",
       MediaSizeGroup::kSizeNamed},
      {{88900, 127000},
       "oe_photo-l_3.5x5in",
       u"3.5 x 5 in",
       MediaSizeGroup::kSizeIn},
      {{55000, 91000},
       "om_business-card_55x91mm",
       u"55 x 91 mm",
       MediaSizeGroup::kSizeMm},
  };

  for (const auto& test_case : kTestCases) {
    VerifyLocalizedInfo(test_case);
  }
}

// Verifies that we generate a sensible vendor ID and display name when no
// localization is found for a given media size.
TEST(PrintMediaL10N, LocalizeNonStandardSizes) {
  const MediaInfoTestCase kTestCases[] = {
      {{310000, 410000},
       "om_310000x410000um_310x410mm",
       u"310 x 410 mm",
       MediaSizeGroup::kSizeMm},
      {{101600, 177800},
       "om_101600x177800um_101x177mm",
       u"4 x 7 in",
       MediaSizeGroup::kSizeIn},
      {{101600, 180620},
       "om_101600x180620um_101x180mm",
       u"4 x 7.111 in",
       MediaSizeGroup::kSizeIn},
      {{209900, 297040},
       "om_209900x297040um_209x297mm",
       u"210 x 297 mm",
       MediaSizeGroup::kSizeMm},
      {{200030, 148170},
       "om_200030x148170um_200x148mm",
       u"200 x 148 mm",
       MediaSizeGroup::kSizeMm},
      {{203200, 266700},
       "om_203200x266700um_203x266mm",
       u"8 x 10.5 in",
       MediaSizeGroup::kSizeIn},
      {{133350, 180620},
       "om_133350x180620um_133x180mm",
       u"5.25 x 7.111 in",
       MediaSizeGroup::kSizeIn},
  };

  for (const auto& test_case : kTestCases) {
    VerifyLocalizedInfo(test_case);
  }
}

// Verifies that paper sizes are returned in the expected order of groups.
TEST(PrintMediaL10N, SortGroupsOrdered) {
  PaperWithSizeInfo mm = {MediaSizeInfo{"", u"mm", MediaSizeGroup::kSizeMm},
                          Paper{"metric", "mm", gfx::Size()}};
  PaperWithSizeInfo in = {MediaSizeInfo{"", u"in", MediaSizeGroup::kSizeIn},
                          Paper{"inches", "in", gfx::Size()}};
  PaperWithSizeInfo named = {
      MediaSizeInfo{"", u"named", MediaSizeGroup::kSizeNamed},
      Paper{"named size", "named", gfx::Size()}};

  std::vector<PaperWithSizeInfo> papers = {mm, named, in};
  std::vector<PaperWithSizeInfo> expected = {in, mm, named};
  SortPaperDisplayNames(papers);
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

// Verifies that inch paper sizes are sorted by width, then height.
TEST(PrintMediaL10N, SortInchSizes) {
  PaperWithSizeInfo p1 = {
      MediaSizeInfo{"", u"1x3", MediaSizeGroup::kSizeIn},
      Paper{"1x3", "in", gfx::Size(1, 3), gfx::Rect(0, 0, 1, 3)}};
  PaperWithSizeInfo p2 = {
      MediaSizeInfo{"", u"2x1", MediaSizeGroup::kSizeIn},
      Paper{"2x1", "in", gfx::Size(2, 1), gfx::Rect(0, 0, 2, 1)}};
  PaperWithSizeInfo p3 = {
      MediaSizeInfo{"", u"2x2", MediaSizeGroup::kSizeIn},
      Paper{"2x2", "in", gfx::Size(2, 2), gfx::Rect(0, 0, 2, 2)}};

  std::vector<PaperWithSizeInfo> papers = {p2, p3, p1};
  std::vector<PaperWithSizeInfo> expected = {p1, p2, p3};
  SortPaperDisplayNames(papers);
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

// Verifies that mm paper sizes are sorted by width, then height.
TEST(PrintMediaL10N, SortMmSizes) {
  PaperWithSizeInfo p1 = {
      MediaSizeInfo{"", u"1x3", MediaSizeGroup::kSizeMm},
      Paper{"1x3", "mm", gfx::Size(1, 3), gfx::Rect(0, 0, 1, 3)}};
  PaperWithSizeInfo p2 = {
      MediaSizeInfo{"", u"2x1", MediaSizeGroup::kSizeMm},
      Paper{"2x1", "mm", gfx::Size(2, 1), gfx::Rect(0, 0, 2, 1)}};
  PaperWithSizeInfo p3 = {
      MediaSizeInfo{"", u"2x2", MediaSizeGroup::kSizeMm},
      Paper{"2x2", "mm", gfx::Size(2, 2), gfx::Rect(0, 0, 2, 2)}};

  std::vector<PaperWithSizeInfo> papers = {p2, p3, p1};
  std::vector<PaperWithSizeInfo> expected = {p1, p2, p3};
  SortPaperDisplayNames(papers);
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

// Verifies that named paper sizes are sorted by name, width, height.
TEST(PrintMediaL10N, SortNamedSizes) {
  PaperWithSizeInfo p1 = {
      MediaSizeInfo{"", u"AAA", MediaSizeGroup::kSizeNamed},
      Paper{"AAA", "name", gfx::Size(50, 50), gfx::Rect(0, 0, 50, 50)}};
  PaperWithSizeInfo p2 = {
      MediaSizeInfo{"", u"BBB", MediaSizeGroup::kSizeNamed},
      Paper{"BBB", "name1", gfx::Size(1, 3), gfx::Rect(0, 0, 1, 3)}};
  PaperWithSizeInfo p3 = {
      MediaSizeInfo{"", u"BBB", MediaSizeGroup::kSizeNamed},
      Paper{"BBB", "name2", gfx::Size(2, 2), gfx::Rect(0, 0, 2, 2)}};
  PaperWithSizeInfo p4 = {
      MediaSizeInfo{"", u"BBB", MediaSizeGroup::kSizeNamed},
      Paper{"BBB", "name3", gfx::Size(2, 3), gfx::Rect(0, 0, 2, 3)}};

  std::vector<PaperWithSizeInfo> papers = {p4, p1, p2, p3};
  std::vector<PaperWithSizeInfo> expected = {p1, p2, p3, p4};
  SortPaperDisplayNames(papers);
  for (size_t i = 0; i < expected.size(); i++) {
    VerifyPaperSizeMatch(papers[i], expected[i]);
  }
}

}  // namespace printing
