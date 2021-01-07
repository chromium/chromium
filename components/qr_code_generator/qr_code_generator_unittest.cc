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
  QRCodeGenerator qr;
  uint8_t input[QRCodeGenerator::V5::kInputBytes];
  memset(input, 'a', sizeof(input));
  base::Optional<QRCodeGenerator::GeneratedCode> qr_code = qr.Generate(input);
  ASSERT_NE(qr_code, base::nullopt);
  auto& qr_data = qr_code->data;

  int index = 0;
  ASSERT_EQ(qr_data.size(),
            static_cast<size_t>(QRCodeGenerator::V5::kTotalSize));
  for (int y = 0; y < QRCodeGenerator::V5::kSize; y++) {
    for (int x = 0; x < QRCodeGenerator::V5::kSize; x++) {
      ASSERT_EQ(0, qr_data[index++] & 0b11111100);
    }
  }
}

TEST(QRCodeGenerator, GenerateVersionSelection) {
  // Ensure that dynamic version selection works,
  // even when reusing the same QR Generator object.
  // Longer inputs produce longer codes.
  QRCodeGenerator qr;

  uint8_t input_small[10];
  memset(input_small, 'a', sizeof(input_small));
  base::Optional<QRCodeGenerator::GeneratedCode> qr_code_small(
      qr.Generate(input_small));
  ASSERT_NE(qr_code_small, base::nullopt);
  int size_small = qr_code_small->data.size();

  uint8_t input_large[100];
  memset(input_large, 'a', sizeof(input_large));
  base::Optional<QRCodeGenerator::GeneratedCode> qr_code_large(
      qr.Generate(input_large));
  ASSERT_NE(qr_code_large, base::nullopt);
  int size_large = qr_code_large->data.size();

  ASSERT_GT(size_small, 0);
  ASSERT_GT(size_large, 0);
  ASSERT_GT(size_large, size_small);
}
