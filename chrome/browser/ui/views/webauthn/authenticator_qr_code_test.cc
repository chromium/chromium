// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_code.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(AuthenticatorQRCode, Generate) {
  // Without a QR decoder implementation, there's a limit to how much we can
  // test the QR encoder. Therefore this test just runs a generation to ensure
  // that no DCHECKs are hit and that the output has the correct structure. When
  // run under ASan, this will also check that every byte of the output has been
  // written to.
  AuthenticatorQRCode qr;
  uint8_t input[AuthenticatorQRCode::kInputBytes];
  memset(input, 'a', sizeof(input));
  auto qr_data = qr.Generate(input);

  int index = 0;
  for (int y = 0; y < AuthenticatorQRCode::kSize; y++) {
    for (int x = 0; x < AuthenticatorQRCode::kSize; x++) {
      ASSERT_EQ(0, qr_data[index++] & 0b11111100);
    }
  }
}
