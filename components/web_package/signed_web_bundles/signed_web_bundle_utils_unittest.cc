// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr uint8_t kFakeUnsignedWebBundleHash[] = {0x01, 0x02, 0x03};
constexpr uint8_t kFakeIntegrityBlock[] = {0x04, 0x05, 0x06, 0x07};
constexpr uint8_t kFakeAttributes[] = {0x08, 0x09};

constexpr uint8_t kExpectedPayloadForSigning[] = {
    // length
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    // unsigned web bundle hash
    0x01, 0x02, 0x03,
    // length
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
    // integrity block
    0x04, 0x05, 0x06, 0x07,
    // length
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    // attributes
    0x08, 0x09};

}  // namespace

TEST(SignedWebBundleUtilsTest, BuildSignaturePayload) {
  auto payload = CreateSignaturePayload(kFakeUnsignedWebBundleHash,
                                        kFakeIntegrityBlock, kFakeAttributes);
  EXPECT_EQ(payload, std::vector(std::begin(kExpectedPayloadForSigning),
                                 std::end(kExpectedPayloadForSigning)));
}

}  // namespace web_package
