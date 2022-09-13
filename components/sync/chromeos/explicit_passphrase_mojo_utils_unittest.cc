// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/chromeos/explicit_passphrase_mojo_utils.h"

#include <string>

#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::Eq;
using testing::IsNull;
using testing::NotNull;

TEST(ExplicitPassphraseMojoUtilsTest, ShouldConvertNigoriToMojoAndBack) {
  auto nigori1 = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori1, NotNull());

  auto mojo_nigori_key = NigoriToMojo(*nigori1);
  ASSERT_FALSE(mojo_nigori_key.is_null());

  auto nigori2 = NigoriFromMojo(*mojo_nigori_key);
  ASSERT_THAT(nigori2, NotNull());

  std::string deprecated_user_key1;
  std::string encryption_key1;
  std::string mac_key1;
  nigori1->ExportKeys(&deprecated_user_key1, &encryption_key1, &mac_key1);

  std::string deprecated_user_key2;
  std::string encryption_key2;
  std::string mac_key2;
  nigori2->ExportKeys(&deprecated_user_key2, &encryption_key2, &mac_key2);
  // Don't check user key, because it's deprecated and safe to ignore.
  EXPECT_THAT(encryption_key1, Eq(encryption_key2));
  EXPECT_THAT(mac_key1, Eq(mac_key2));
}

TEST(ExplicitPassphraseMojoUtilsTest, ShouldFailMojoToNigoriIfMojoEmpty) {
  auto empty_mojo_nigori_key = crosapi::mojom::NigoriKey::New();
  EXPECT_THAT(NigoriFromMojo(*empty_mojo_nigori_key), IsNull());
}

TEST(ExplicitPassphraseMojoUtilsTest, ShouldFailMojoToNigoriIfMojoNotValid) {
  auto invalid_mojo_nigori_key = crosapi::mojom::NigoriKey::New();
  invalid_mojo_nigori_key->encryption_key = {1, 2, 3};
  invalid_mojo_nigori_key->mac_key = {1, 2, 3};
  EXPECT_THAT(NigoriFromMojo(*invalid_mojo_nigori_key), IsNull());
}

}  // namespace

}  // namespace syncer
