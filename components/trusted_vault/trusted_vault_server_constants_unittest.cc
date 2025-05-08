// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_server_constants.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::Eq;

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
