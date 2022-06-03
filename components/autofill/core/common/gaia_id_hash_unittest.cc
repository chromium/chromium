// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/gaia_id_hash.h"

#include "base/base64.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(GaiaIdHashTest, ShouldBeDeterministic) {
  const std::string gaia_id = "user_gaia_id";
  GaiaIdHash gaia_id_hash1 = GaiaIdHash::FromGaiaId(gaia_id);
  GaiaIdHash gaia_id_hash2 = GaiaIdHash::FromGaiaId(gaia_id);
  EXPECT_EQ(gaia_id_hash1.ToBinary(), gaia_id_hash2.ToBinary());
  EXPECT_EQ(gaia_id_hash1.ToBase64(), gaia_id_hash2.ToBase64());
  EXPECT_EQ(gaia_id_hash1, gaia_id_hash2);
}

TEST(GaiaIdHashTest, ShouldHash) {
  const std::string gaia_id = "user_gaia_id";
  const std::string gaia_id_hash = crypto::SHA256HashString(gaia_id);
  std::string gaia_id_base64_hash;
  base::Base64Encode(gaia_id_hash, &gaia_id_base64_hash);

  GaiaIdHash hash = GaiaIdHash::FromGaiaId(gaia_id);
  EXPECT_EQ(gaia_id_hash, hash.ToBinary());
  EXPECT_EQ(gaia_id_base64_hash, hash.ToBase64());
}

TEST(GaiaIdHashTest, ShouldBase64EncodeDecode) {
  const std::string gaia_id = "user_gaia_id";
  GaiaIdHash hash1 = GaiaIdHash::FromGaiaId(gaia_id);
  GaiaIdHash hash2 = GaiaIdHash::FromBase64(hash1.ToBase64());
  EXPECT_EQ(hash1, hash2);
}

TEST(GaiaIdHashTest, ShouldBeInvalid) {
  // Hash must be of length crypto::kSHA256Length.
  GaiaIdHash hash1 = GaiaIdHash::FromBinary("too_short_hash");
  EXPECT_FALSE(hash1.IsValid());

  GaiaIdHash hash2 = GaiaIdHash::FromBase64("invalid_base64");
  EXPECT_FALSE(hash2.IsValid());
}

}  // namespace autofill
