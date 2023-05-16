// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/passkey_credential.h"

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kRpId[] = "gensokyo.com";

constexpr std::array<const uint8_t, 4> kCredentialId1 = {'a', 'b', 'c', 'd'};
constexpr std::array<const uint8_t, 4> kUserId1 = {'1', '2', '3', '4'};
constexpr char kUserName1[] = "reimu";
constexpr char kUserDisplayName1[] = "Reimu Hakurei";

constexpr std::array<const uint8_t, 4> kCredentialId2 = {'e', 'f', 'g', 'h'};
constexpr std::array<const uint8_t, 4> kUserId2 = {'5', '6', '7', '8'};
constexpr char kUserName2[] = "marisa";
constexpr char kUserDisplayName2[] = "Marisa Kirisame";

constexpr std::array<const uint8_t, 4> kCredentialIdShadow = {'i', 'j', 'k',
                                                              'l'};

std::vector<uint8_t> ToUint8Vector(
    const std::array<const uint8_t, 4>& byte_array) {
  return std::vector<uint8_t>(byte_array.begin(), byte_array.end());
}

}  // namespace

class PasskeyCredentialTest : public testing::Test {};

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
  credential1_shadow.set_credential_id(kCredentialIdShadow.data(),
                                       kCredentialIdShadow.size());
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

  ASSERT_THAT(credentials,
              testing::UnorderedElementsAre(
                  PasskeyCredential(PasskeyCredential::Source::kAndroidPhone,
                                    kRpId, ToUint8Vector(kCredentialIdShadow),
                                    ToUint8Vector(kUserId1), kUserName1,
                                    kUserDisplayName1),
                  PasskeyCredential(PasskeyCredential::Source::kAndroidPhone,
                                    kRpId, ToUint8Vector(kCredentialId2),
                                    ToUint8Vector(kUserId2), kUserName2,
                                    kUserDisplayName2)));
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
          PasskeyCredential::Source::kAndroidPhone, kRpId,
          ToUint8Vector(kCredentialId1), ToUint8Vector(kUserId1), "", "")));
}

}  // namespace password_manager
