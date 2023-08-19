// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/passkey_credential.h"

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager {

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

constexpr char kRpId[] = "gensokyo.com";

constexpr std::array<const uint8_t, 4> kCredentialId1 = {'a', 'b', 'c', 'd'};
constexpr std::array<const uint8_t, 4> kUserId1 = {'1', '2', '3', '4'};
constexpr char kUserName1[] = "reimu";
constexpr char kUserDisplayName1[] = "Reimu Hakurei";

constexpr std::array<const uint8_t, 4> kCredentialId2 = {'e', 'f', 'g', 'h'};
constexpr std::array<const uint8_t, 4> kUserId2 = {'5', '6', '7', '8'};
constexpr char kUserName2[] = "marisa";
constexpr char kUserDisplayName2[] = "Marisa Kirisame";

constexpr std::array<const uint8_t, 4> kCredentialIdShadow1 = {'i', 'j', 'k'};
constexpr std::array<const uint8_t, 4> kCredentialIdShadow2 = {'l', 'm', 'n'};

std::vector<uint8_t> ToUint8Vector(
    const std::array<const uint8_t, 4>& byte_array) {
  return std::vector<uint8_t>(byte_array.begin(), byte_array.end());
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

class PasskeyCredentialTest : public testing::Test {};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PasskeyCredentialTest, FromCredentialSpecifics) {
  sync_pb::WebauthnCredentialSpecifics credential1;
  credential1.set_sync_id(base::RandBytesAsString(16));
  credential1.set_credential_id(kCredentialId1.data(), kCredentialId1.size());
  credential1.set_rp_id(kRpId);
  credential1.set_user_id(kUserId1.data(), kUserId1.size());
  credential1.set_user_name(kUserName1);
  credential1.set_user_display_name(kUserDisplayName1);

  sync_pb::WebauthnCredentialSpecifics credential2;
  credential2.set_sync_id(base::RandBytesAsString(16));
  credential2.set_credential_id(kCredentialId2.data(), kCredentialId2.size());
  credential2.set_rp_id(kRpId);
  credential2.set_user_id(kUserId2.data(), kUserId2.size());
  credential2.set_user_name(kUserName2);
  credential2.set_user_display_name(kUserDisplayName2);

  // Shadow the first credential.
  sync_pb::WebauthnCredentialSpecifics credential1_shadow;
  credential1_shadow.set_sync_id(base::RandBytesAsString(16));
  credential1_shadow.set_credential_id(kCredentialIdShadow1.data(),
                                       kCredentialIdShadow1.size());
  credential1_shadow.set_rp_id(kRpId);
  credential1_shadow.set_user_id(kUserId1.data(), kUserId1.size());
  credential1_shadow.set_user_name(kUserName1);
  credential1_shadow.set_user_display_name(kUserDisplayName1);
  credential1_shadow.add_newly_shadowed_credential_ids(
      credential1.credential_id());

  std::vector<PasskeyCredential> credentials =
      PasskeyCredential::FromCredentialSpecifics(std::vector{
          credential1,
          credential2,
          credential1_shadow,
      });

  ASSERT_THAT(
      credentials,
      testing::UnorderedElementsAre(
          PasskeyCredential(PasskeyCredential::Source::kAndroidPhone,
                            PasskeyCredential::RpId(kRpId),
                            PasskeyCredential::CredentialId(
                                ToUint8Vector(kCredentialIdShadow1)),
                            PasskeyCredential::UserId(ToUint8Vector(kUserId1)),
                            PasskeyCredential::Username(kUserName1),
                            PasskeyCredential::DisplayName(kUserDisplayName1)),
          PasskeyCredential(
              PasskeyCredential::Source::kAndroidPhone,
              PasskeyCredential::RpId(kRpId),
              PasskeyCredential::CredentialId(ToUint8Vector(kCredentialId2)),
              PasskeyCredential::UserId(ToUint8Vector(kUserId2)),
              PasskeyCredential::Username(kUserName2),
              PasskeyCredential::DisplayName(kUserDisplayName2))));
}

// Regression test for crbug.com/1447116.
TEST_F(PasskeyCredentialTest, ImplicitlyShadowedCredentials) {
  // Create two pairs of credentials. Each pair shares the same rp id and user
  // id, so the newer credential should implicitly shadow the older one.
  sync_pb::WebauthnCredentialSpecifics credential1;
  credential1.set_credential_id(kCredentialId1.data(), kCredentialId1.size());
  credential1.set_rp_id(kRpId);
  credential1.set_user_id(kUserId1.data(), kUserId1.size());
  credential1.set_creation_time(200);

  sync_pb::WebauthnCredentialSpecifics shadowed_credential1;
  shadowed_credential1.set_credential_id(kCredentialIdShadow1.data(),
                                         kCredentialIdShadow1.size());
  shadowed_credential1.set_rp_id(kRpId);
  shadowed_credential1.set_user_id(kUserId1.data(), kUserId1.size());
  shadowed_credential1.set_creation_time(100);

  sync_pb::WebauthnCredentialSpecifics credential2;
  credential2.set_credential_id(kCredentialId2.data(), kCredentialId2.size());
  credential2.set_rp_id(kRpId);
  credential2.set_user_id(kUserId2.data(), kUserId2.size());
  credential2.set_creation_time(400);

  sync_pb::WebauthnCredentialSpecifics shadowed_credential2;
  shadowed_credential2.set_credential_id(kCredentialIdShadow2.data(),
                                         kCredentialIdShadow2.size());
  shadowed_credential2.set_rp_id(kRpId);
  shadowed_credential2.set_user_id(kUserId2.data(), kUserId2.size());
  shadowed_credential2.set_creation_time(300);

  std::vector<PasskeyCredential> credentials =
      PasskeyCredential::FromCredentialSpecifics(std::vector{
          credential1,
          shadowed_credential2,
          credential2,
          shadowed_credential1,
      });

  ASSERT_THAT(
      credentials,
      testing::UnorderedElementsAre(
          PasskeyCredential(
              PasskeyCredential::Source::kAndroidPhone,
              PasskeyCredential::RpId(kRpId),
              PasskeyCredential::CredentialId(ToUint8Vector(kCredentialId1)),
              PasskeyCredential::UserId(ToUint8Vector(kUserId1)),
              PasskeyCredential::Username(""),
              PasskeyCredential::DisplayName("")),
          PasskeyCredential(
              PasskeyCredential::Source::kAndroidPhone,
              PasskeyCredential::RpId(kRpId),
              PasskeyCredential::CredentialId(ToUint8Vector(kCredentialId2)),
              PasskeyCredential::UserId(ToUint8Vector(kUserId2)),
              PasskeyCredential::Username(""),
              PasskeyCredential::DisplayName(""))));
}

TEST_F(PasskeyCredentialTest, FromCredentialSpecifics_EmptyOptionalFields) {
  sync_pb::WebauthnCredentialSpecifics credential;
  credential.set_sync_id(base::RandBytesAsString(16));
  credential.set_credential_id(kCredentialId1.data(), kCredentialId1.size());
  credential.set_rp_id(kRpId);
  credential.set_user_id(kUserId1.data(), kUserId1.size());

  ASSERT_THAT(
      PasskeyCredential::FromCredentialSpecifics(std::vector{credential}),
      testing::UnorderedElementsAre(PasskeyCredential(
          PasskeyCredential::Source::kAndroidPhone,
          PasskeyCredential::RpId(kRpId),
          PasskeyCredential::CredentialId(ToUint8Vector(kCredentialId1)),
          PasskeyCredential::UserId(ToUint8Vector(kUserId1)),
          PasskeyCredential::Username(""),
          PasskeyCredential::DisplayName(""))));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(PasskeyCredentialTest, GetAuthenticatorLabel) {
  PasskeyCredential credential(PasskeyCredential::Source::kAndroidPhone,
                               PasskeyCredential::RpId("rpid.com"),
                               PasskeyCredential::CredentialId({1, 2, 3, 4}),
                               PasskeyCredential::UserId({5, 6, 7, 8}));
  EXPECT_EQ(credential.GetAuthenticatorLabel(),
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_SCREEN_LOCK));
  std::u16string authenticator_label = u"Reimu's phone";
  credential.set_authenticator_label(authenticator_label);
  EXPECT_EQ(credential.GetAuthenticatorLabel(), authenticator_label);
}

}  // namespace password_manager
