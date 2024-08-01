// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/qr_code_generator/qr_code_generator.h"

#include <limits>
#include <optional>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace qr_code_generator {

TEST(QRCodeGeneratorTest, Generate) {
  // Without a QR decoder implementation, there's a limit to how much we can
  // test the QR encoder. Therefore this test just runs a generation to ensure
  // that no DCHECKs are hit and that the output has the correct structure. When
  // run under ASan, this will also check that every byte of the output has been
  // written to.

  constexpr size_t kMaxInputLen = 210;
  uint8_t input[kMaxInputLen];
  std::optional<int> smallest_size;
  std::optional<int> largest_size;

  for (const bool use_alphanum : {false, true}) {
    SCOPED_TRACE(use_alphanum);
    // 'A' is in the alphanumeric set, but 'a' is not.
    memset(input, use_alphanum ? 'A' : 'a', sizeof(input));

    for (size_t input_len = 30; input_len < kMaxInputLen; input_len += 10) {
      SCOPED_TRACE(input_len);

      base::expected<GeneratedCode, Error> qr_code =
          GenerateCode(base::span<const uint8_t>(input, input_len));
      ASSERT_TRUE(qr_code.has_value());
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

TEST(QRCodeGeneratorTest, ManySizes) {
  // Test multiple input sizes.  This test was originally designed to test for
  // memory safety problems caused by off-by-one bugs in the old C++
  // implementation. We are now shipping a memory-safe Rust implementation so we
  // are now testing only sizes up to 90 - this helps to avoid flaky test
  // timeouts.
  std::string input = "";
  std::map<int, size_t> max_input_length_for_qr_size;

  for (;;) {
    input.push_back('!');
    if (input.size() > 90) {
      break;
    }

    base::expected<GeneratedCode, Error> code =
        GenerateCode(base::as_byte_span(input));
    ASSERT_TRUE(code.has_value());
    max_input_length_for_qr_size[code->qr_size] = input.size();
  }

  // Capacities taken from https://www.qrcode.com/en/about/version.html
  //
  // Rust supports all QR versions from 1 to 40 and defaults to M error
  // correction.
  //
  // Other versions skipped, because otherwise the test may timeout.
  EXPECT_EQ(max_input_length_for_qr_size[21], 14u);  // 1-M
  EXPECT_EQ(max_input_length_for_qr_size[25], 26u);  // 2-M
  EXPECT_EQ(max_input_length_for_qr_size[29], 42u);  // 3-M
  EXPECT_EQ(max_input_length_for_qr_size[33], 62u);  // 4-M
  EXPECT_EQ(max_input_length_for_qr_size[37], 84u);  // 5-M
}

// Test helper that returns `GeneratedCode::qr_size` or -1 if there was a
// failure.
int GenerateAndGetQrCodeSize(size_t input_size) {
  std::string input(input_size, '!');

  base::expected<GeneratedCode, Error> code =
      GenerateCode(base::as_byte_span(input));
  return code.has_value() ? code->qr_size : -1;
}

TEST(QRCodeGeneratorTest, InputSize106) {
  EXPECT_EQ(41, GenerateAndGetQrCodeSize(106u));  // 6-M
}

TEST(QRCodeGeneratorTest, InputSize122) {
  EXPECT_EQ(45, GenerateAndGetQrCodeSize(122u));  // 7-M
}

TEST(QRCodeGeneratorTest, InputSize180) {
  EXPECT_EQ(53, GenerateAndGetQrCodeSize(180u));  // 9-M
}

TEST(QRCodeGeneratorTest, InputSize287) {
  EXPECT_EQ(65, GenerateAndGetQrCodeSize(287u));  // 12-M
}

TEST(QRCodeGeneratorTest, InputSize666) {
  EXPECT_EQ(97, GenerateAndGetQrCodeSize(666u));  // 20-M
}

TEST(QRCodeGeneratorTest, HugeInput) {
  // The numbers below have been taken from
  // https://www.qrcode.com/en/about/version.html, for version = 40,
  // ECC level = M.
  const size_t kMaxInputSizeForNumericInputVersion40 = 5596;
  const size_t kMaxInputSizeForBinaryInputVersion40 = 2331;

  std::vector<uint8_t> huge_numeric_input(kMaxInputSizeForNumericInputVersion40,
                                          '0');
  std::vector<uint8_t> huge_binary_input(kMaxInputSizeForBinaryInputVersion40,
                                         '\0');


  // The Rust implementation can generate QR codes up to version 40.
  ASSERT_TRUE(GenerateCode(huge_numeric_input).has_value());
  ASSERT_TRUE(GenerateCode(huge_binary_input).has_value());

  // Adding another character means that the inputs will no longer fit into QR
  // code version 40 (as of year 2023 there are no further versions defined by
  // the spec).
  {
    huge_numeric_input.push_back('0');
    auto failure = GenerateCode(huge_numeric_input);
    ASSERT_FALSE(failure.has_value());
    EXPECT_EQ(failure.error(), Error::kInputTooLong);
  }
  {
    huge_binary_input.push_back('\0');
    auto failure = GenerateCode(huge_binary_input);
    ASSERT_FALSE(failure.has_value());
    EXPECT_EQ(failure.error(), Error::kInputTooLong);
  }
}

TEST(QRCodeGeneratorTest, InvalidMinVersion) {
  std::vector<uint8_t> input(123);  // Arbitrary valid input.

  {
    auto failure = GenerateCode(input, std::make_optional(41));
    ASSERT_FALSE(failure.has_value());
    EXPECT_EQ(failure.error(), Error::kUnknownError);
  }

  {
    auto failure = GenerateCode(
        input, std::make_optional(std::numeric_limits<int>::max()));
    ASSERT_FALSE(failure.has_value());
    EXPECT_EQ(failure.error(), Error::kUnknownError);
  }

  {
    auto failure = GenerateCode(input, std::make_optional(-1));
    ASSERT_FALSE(failure.has_value());
    EXPECT_EQ(failure.error(), Error::kUnknownError);
  }
}

}  // namespace qr_code_generator
