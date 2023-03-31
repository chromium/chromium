// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/passkey_credential.h"

#include <string>

#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager {

constexpr char kUsername[] = "lora";
const char16_t kUsernameUtf16[] = u"lora";
const char16_t kDeviceName[] = u"Lora's Pixel 7";
constexpr char kBackendId[] = "1234";

class PasskeyCredentialTest : public testing::Test {};

TEST_F(PasskeyCredentialTest, CreateCredential) {
  PasskeyCredential credential((PasskeyCredential::Username(kUsername)),
                               PasskeyCredential::DeviceName(kDeviceName),
                               PasskeyCredential::BackendId(kBackendId));
  EXPECT_EQ(credential.username(), kUsernameUtf16);
  EXPECT_EQ(credential.device_name(), kDeviceName);
  EXPECT_EQ(credential.id(), kBackendId);
}

TEST_F(PasskeyCredentialTest, CreateCredentialWithNoUsername) {
  PasskeyCredential credential((PasskeyCredential::Username(absl::nullopt)),
                               PasskeyCredential::DeviceName(kDeviceName),
                               PasskeyCredential::BackendId(kBackendId));
  EXPECT_EQ(credential.username(),
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN));
}

TEST_F(PasskeyCredentialTest, CreateCredentialWithEmptyUsername) {
  PasskeyCredential credential((PasskeyCredential::Username("")),
                               PasskeyCredential::DeviceName(kDeviceName),
                               PasskeyCredential::BackendId(kBackendId));
  EXPECT_EQ(credential.username(),
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN));
}

}  // namespace password_manager
