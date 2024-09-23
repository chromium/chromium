// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_server_constants.h"

#include <cstdint>
#include <vector>

#include "components/trusted_vault/securebox.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::Eq;

TEST(TrustedVaultServerConstantsTest, ShouldGetGetSecurityDomainMemberURL) {
  const GURL kTestUrl("https://example.com/v1/");

  // Arbitrary key, with an appropriate length.
  const std::vector<uint8_t> kPublicKey{
      0x4,  0xF2, 0x4C, 0x45, 0xBA, 0xF4, 0xF8, 0x6C, 0xF9, 0x73, 0xCE,
      0x75, 0xC,  0xC9, 0xD4, 0xF,  0x4A, 0x53, 0xB7, 0x85, 0x46, 0x41,
      0xFB, 0x31, 0x17, 0xF,  0xEB, 0xB,  0x45, 0xE4, 0x29, 0x69, 0x9B,
      0xB2, 0x7,  0x12, 0xC1, 0x9,  0x3D, 0xEF, 0xBB, 0x57, 0xDC, 0x56,
      0x12, 0x29, 0xF2, 0x73, 0xE1, 0xC5, 0x99, 0x1C, 0x49, 0x3A, 0xA2,
      0x30, 0xF9, 0xBA, 0x3B, 0xB1, 0x83, 0xCF, 0x1B, 0x5D, 0xE8};

  // Guard against future code changes, in case the key length changes.
  ASSERT_THAT(kPublicKey.size(), Eq(SecureBoxKeyPair::GenerateRandom()
                                        ->public_key()
                                        .ExportToBytes()
                                        .size()));

  // Note that production code (TrustedVaultRequest::CreateURLLoader) will
  // append &alt=proto to the URL.
  EXPECT_THAT(GetGetSecurityDomainMemberURL(kTestUrl, kPublicKey).spec(),
              Eq("https://example.com/v1/users/me/members/"
                 "BPJMRbr0-Gz5c851DMnUD0pTt4VGQfsxFw_"
                 "rC0XkKWmbsgcSwQk977tX3FYSKfJz4cWZHEk6ojD5ujuxg88bXeg"
                 "?view=2"
                 "&request_header.force_master_read=true"));
}

TEST(TrustedVaultServerConstantsTest, GetSecurityDomainByName) {
  EXPECT_THAT(GetSecurityDomainByName("chromesync"),
              Eq(SecurityDomainId::kChromeSync));
  EXPECT_THAT(GetSecurityDomainByName("hw_protected"),
              Eq(SecurityDomainId::kPasskeys));
  EXPECT_THAT(GetSecurityDomainByName("users/me/securitydomains/chromesync"),
              Eq(std::nullopt));
  EXPECT_THAT(GetSecurityDomainByName(""), Eq(std::nullopt));
}

TEST(TrustedVaultServerConstantsTest, GetSecurityDomainName) {
  EXPECT_THAT(GetSecurityDomainName(SecurityDomainId::kChromeSync),
              Eq("chromesync"));
  EXPECT_THAT(GetSecurityDomainName(SecurityDomainId::kPasskeys),
              Eq("hw_protected"));
}

}  // namespace

}  // namespace trusted_vault
