// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/qr_code_generator/qr_code_generator.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/qr_code_generator/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace qr_code_generator {

enum class RustFeatureState { kRustEnabled, kRustDisabled };

class QRCodeGeneratorTest : public testing::TestWithParam<RustFeatureState> {
 public:
  QRCodeGeneratorTest() {
    switch (GetParam()) {
      case RustFeatureState::kRustEnabled:
        features_.InitAndEnableFeature(kRustyQrCodeGeneratorFeature);
        break;
      case RustFeatureState::kRustDisabled:
        features_.InitAndDisableFeature(kRustyQrCodeGeneratorFeature);
        break;
    }
  }

 protected:
  base::test::ScopedFeatureList features_;
};

TEST_P(QRCodeGeneratorTest, Generate) {
  // Without a QR decoder implementation, there's a limit to how much we can
  // test the QR encoder. Therefore this test just runs a generation to ensure
  // that no DCHECKs are hit and that the output has the correct structure. When
  // run under ASan, this will also check that every byte of the output has been
  // written to.

  constexpr size_t kMaxInputLen = 210;
  uint8_t input[kMaxInputLen];
  QRCodeGenerator qr;
  absl::optional<int> smallest_size;
  absl::optional<int> largest_size;

  for (const bool use_alphanum : {false, true}) {
    SCOPED_TRACE(use_alphanum);
    // 'A' is in the alphanumeric set, but 'a' is not.
    memset(input, use_alphanum ? 'A' : 'a', sizeof(input));

    for (size_t input_len = 30; input_len < kMaxInputLen; input_len += 10) {
      SCOPED_TRACE(input_len);

      absl::optional<QRCodeGenerator::GeneratedCode> qr_code =
          qr.Generate(base::span<const uint8_t>(input, input_len));
      ASSERT_NE(qr_code, absl::nullopt);
      auto& qr_data = qr_code->data;

      if (!smallest_size || qr_code->qr_size < *smallest_size) {
        smallest_size = qr_code->qr_size;
      }
      if (!largest_size || qr_code->qr_size > *largest_size) {
        largest_size = qr_code->qr_size;
      }

      int index = 0;
      for (int y = 0; y < qr_code->qr_size; y++) {
        for (int x = 0; x < qr_code->qr_size; x++) {
          ASSERT_EQ(0, qr_data[index++] & 0b11111100);
        }
      }
    }
  }

  // The generator should generate a variety of QR sizes.
  ASSERT_TRUE(smallest_size);
  ASSERT_TRUE(largest_size);
  ASSERT_LT(*smallest_size, *largest_size);
}

TEST_P(QRCodeGeneratorTest, ManySizes) {
  // Generate larger and larger QR codes until there's a clean failure. Ensures
  // that there are no edge cases like crbug.com/1177437.
  QRCodeGenerator qr;
  std::string input = "";
  std::map<int, size_t> max_input_length_for_qr_size;

  for (;;) {
    input.push_back('!');

    // C++ can only handle up to 287 of arbitrary bytes, but Rust implementation
    // wouldn't `break` out of the loop until hitting `kMaxInputSize=700`.
    // Therefore to avoid timeouts we test only a subset of possible input
    // lengths when Rusty QR code generation is enabled.
    if (IsRustyQrCodeGeneratorFeatureEnabled()) {
      // The "holes" below try to ensure coverage of lengths around the test
      // assertions below that are shared across C++ and Rust + around a few
      // longer lengths that Rust can handle.

      bool want_to_cover = input.size() < 130;  // 2-L (2-M in Rust), 5-M, 7-M
      want_to_cover |= ((170 < input.size()) && (input.size() < 190));  // 9-M
      want_to_cover |= ((270 < input.size()) && (input.size() < 300));  // 12-M
      want_to_cover |= ((660 < input.size()) && (input.size() < 670));  // 20-M
      want_to_cover |= ((690 < input.size()) && (input.size() < 710));  // max
      if (!want_to_cover) {
        continue;
      }
    }

    absl::optional<QRCodeGenerator::GeneratedCode> code =
        qr.Generate(base::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(input.data()), input.size()));
    if (!code) {
      break;
    }
    max_input_length_for_qr_size[code->qr_size] = input.size();
  }

  ASSERT_GT(input.size(), 200u);

  // Capacities taken from https://www.qrcode.com/en/about/version.html
  EXPECT_EQ(max_input_length_for_qr_size[37], 84u);   // 5-M
  EXPECT_EQ(max_input_length_for_qr_size[45], 122u);  // 7-M
  EXPECT_EQ(max_input_length_for_qr_size[53], 180u);  // 9-M
  EXPECT_EQ(max_input_length_for_qr_size[65], 287u);  // 12-M (max for C++)
  if (base::FeatureList::IsEnabled(kRustyQrCodeGeneratorFeature)) {
    // Rust supports all QR versions from 1 to 40 and defaults to M error
    // correction.
    EXPECT_EQ(max_input_length_for_qr_size[21], 14u);    // 1-M
    EXPECT_EQ(max_input_length_for_qr_size[25], 26u);    // 2-M
    EXPECT_EQ(max_input_length_for_qr_size[29], 42u);    // 3-M
    EXPECT_EQ(max_input_length_for_qr_size[33], 62u);    // 4-M
                                                         // 5-M covered above
    EXPECT_EQ(max_input_length_for_qr_size[41], 106u);   // 6-M
                                                         // 7-M covered above
                                                         // 9-M covered above
                                                         // 12-M covered above
    EXPECT_EQ(max_input_length_for_qr_size[97], 666u);   // 20-M
    EXPECT_EQ(max_input_length_for_qr_size[101], 700u);  // 21-M (not at max)
    // Other versions skipped - otherwise the test would timeout.
  } else {
    // C++ only supports 5 QR versions: 2-L, 5-M, 7-M, 9-M, 12-M.
    EXPECT_EQ(max_input_length_for_qr_size[25], 32u);  // 2-L
  }
}

TEST(QRCodeGenerator, Segmentation) {
  struct Test {
    QRCodeGenerator::VersionClass vclass;
    const char* input;
    std::vector<std::pair<QRCodeGenerator::SegmentType, const char*>> segments;
  };

  const auto SMALL = QRCodeGenerator::VersionClass::SMALL;
  const auto LARGE = QRCodeGenerator::VersionClass::LARGE;
  const auto D = QRCodeGenerator::SegmentType::DIGIT;
  const auto A = QRCodeGenerator::SegmentType::ALPHANUM;
  const auto B = QRCodeGenerator::SegmentType::BINARY;

  static const std::vector<Test> kTests = {
      {SMALL, "", {}},
      {LARGE, "", {}},
      // Runs of the same class should be a single segment.
      {SMALL, "01234", {{D, "01234"}}},
      {LARGE, "01234", {{D, "01234"}}},
      {SMALL, "abcdef", {{B, "abcdef"}}},
      {LARGE, "abcdef", {{B, "abcdef"}}},
      {SMALL, "ABC", {{A, "ABC"}}},
      {LARGE, "ABC", {{A, "ABC"}}},
      // Where cheaper, different classes should be merged into the wider
      // class.
      {SMALL, "01w", {{B, "01w"}}},
      {SMALL, "w01", {{B, "w01"}}},
      // But merging should only happen when cheaper. The merged version here is
      // 60 bits so the following should be split because that costs only 51
      // bits.
      {SMALL, "01234w", {{D, "01234"}, {B, "w"}}},
      {SMALL, "w01234", {{B, "w"}, {D, "01234"}}},
      // The '0' should be merged into either of the binary blocks and then
      // the binary blocks should be unified together.
      {SMALL, "abcdef0abcdef", {{B, "abcdef0abcdef"}}},
      // Segments should always be merged into the lesser class.
      //    1. First establish that six A/a are enough to justify their own
      //       segment.
      {SMALL, "AAAAAAaaaaaa", {{A, "AAAAAA"}, {B, "aaaaaa"}}},
      //    2. The digit should merge into the ALPHANUM block because it's
      //       cheaper there.
      {SMALL, "AAAAAA0aaaaaa", {{A, "AAAAAA0"}, {B, "aaaaaa"}}},
      //    3. That should happen even with things flipped.
      {SMALL, "aaaaaa0AAAAAA", {{B, "aaaaaa"}, {A, "0AAAAAA"}}},
      // Check that a QR input that we might use is segmented as expected.
      {SMALL, "FIDO:/123412341234", {{A, "FIDO:/"}, {D, "123412341234"}}},
  };

  for (const auto& test : kTests) {
    const std::string input = test.input;
    const std::vector<QRCodeGenerator::Segment> segments =
        QRCodeGenerator::SegmentInput(
            test.vclass, base::span<const uint8_t>(
                             reinterpret_cast<const uint8_t*>(test.input),
                             strlen(test.input)));

    bool match = segments.size() == test.segments.size();
    if (match) {
      size_t offset = 0;

      for (size_t i = 0; i < segments.size(); i++) {
        if (segments[i].type != test.segments[i].first ||
            input.substr(offset, segments[i].length) !=
                test.segments[i].second) {
          match = false;
          break;
        }

        offset += segments[i].length;
      }
    }

    if (!match) {
      auto type_to_char =
          [](QRCodeGenerator::SegmentType segment_type) -> char {
        switch (segment_type) {
          case QRCodeGenerator::SegmentType::DIGIT:
            return 'D';
          case QRCodeGenerator::SegmentType::ALPHANUM:
            return 'A';
          case QRCodeGenerator::SegmentType::BINARY:
            return 'B';
        }
      };

      std::string got = "";
      size_t offset = 0;
      for (const auto& segment : segments) {
        if (!got.empty()) {
          got += " ";
        }

        got += type_to_char(segment.type);
        got += input.substr(offset, segment.length);
        offset += segment.length;
      }

      std::string want = "";
      for (const auto& segment : test.segments) {
        if (!want.empty()) {
          want += " ";
        }

        want += type_to_char(segment.first);
        want += segment.second;
      }

      ADD_FAILURE() << "got:  " << got << "\nwant: " << want;
      return;
    }
  }
}

TEST(QRCodeGenerator, SegmentationValid) {
  // The segmentation must always be valid: i.e. must assign each input byte
  // to a segment that can express it, and the segments must span the whole
  // input.
  for (int i = 0; i < 10000; i++) {
    const size_t len = base::RandInt(1, 64);
    std::vector<uint8_t> input(len);
    for (size_t j = 0; j < len; j++) {
      input[j] = base::RandInt(32, 126);
    }

    const std::vector<QRCodeGenerator::Segment> segments =
        QRCodeGenerator::SegmentInput(
            i & 1 ? QRCodeGenerator::VersionClass::SMALL
                  : QRCodeGenerator::VersionClass::LARGE,
            input);

    ASSERT_TRUE(QRCodeGenerator::IsValidSegmentation(segments, input));
    ASSERT_TRUE(QRCodeGenerator::NoSuperfluousSegments(segments));
  }
}

TEST_P(QRCodeGeneratorTest, HugeInput) {
  std::vector<uint8_t> huge_input(QRCodeGenerator::kMaxInputSize + 1);
  QRCodeGenerator qr;
  ASSERT_FALSE(qr.Generate(huge_input));
}

#if BUILDFLAG(ENABLE_RUST_QR)
INSTANTIATE_TEST_SUITE_P(RustEnabled,
                         QRCodeGeneratorTest,
                         ::testing::Values(RustFeatureState::kRustEnabled));
#endif

INSTANTIATE_TEST_SUITE_P(RustDisabled,
                         QRCodeGeneratorTest,
                         ::testing::Values(RustFeatureState::kRustDisabled));

}  // namespace qr_code_generator
