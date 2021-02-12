// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/qr_code_generator/qr_code_generator.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(QRCodeGenerator, Generate) {
  // Without a QR decoder implementation, there's a limit to how much we can
  // test the QR encoder. Therefore this test just runs a generation to ensure
  // that no DCHECKs are hit and that the output has the correct structure. When
  // run under ASan, this will also check that every byte of the output has been
  // written to.

  constexpr size_t kMaxInputLen = 210;
  uint8_t input[kMaxInputLen];
  QRCodeGenerator qr;
  base::Optional<int> smallest_size;
  base::Optional<int> largest_size;

  for (const bool use_alphanum : {false, true}) {
    SCOPED_TRACE(use_alphanum);
    // 'A' is in the alphanumeric set, but 'a' is not.
    memset(input, use_alphanum ? 'A' : 'a', sizeof(input));

    for (size_t input_len = 30; input_len < kMaxInputLen; input_len += 10) {
      SCOPED_TRACE(input_len);

      base::Optional<QRCodeGenerator::GeneratedCode> qr_code =
          qr.Generate(base::span<const uint8_t>(input, input_len));
      ASSERT_NE(qr_code, base::nullopt);
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

TEST(QRCodeGenerator, ManySizes) {
  // Generate larger and larger QR codes until there's a clean failure. Ensures
  // that there are no edge cases like crbug.com/1177437.
  QRCodeGenerator qr;
  std::string input = "!";

  for (size_t i = input.size();; i++) {
    if (!qr.Generate(base::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(input.data()), input.size()))) {
      break;
    }

    input.push_back('!');
  }

  ASSERT_GT(input.size(), 200u);
}
