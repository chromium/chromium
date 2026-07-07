// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_id.h"

#include <concepts>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace unexportable_keys {

TEST(UnexportableSigningKeyIdTest, DefaultConstructor) {
  UnexportableSigningKeyId signing_key_id;
  EXPECT_FALSE(signing_key_id.value().is_empty());
}


TEST(UnexportableAttestationKeyIdTest, DefaultConstructor) {
  UnexportableAttestationKeyId attestation_key_id;
  EXPECT_FALSE(attestation_key_id.value().is_empty());
}

TEST(UnexportableAttestationKeyIdTest, ExplicitConversionFromBase) {
  static_assert(!std::convertible_to<UnexportableSigningKeyId,
                                     UnexportableAttestationKeyId>,
                "Implicit conversion from UnexportableSigningKeyId to "
                "UnexportableAttestationKeyId should not be allowed");

  UnexportableSigningKeyId base_id;
  UnexportableAttestationKeyId derived_id(base_id);
  EXPECT_EQ(derived_id, base_id);
}

TEST(UnexportableAttestationKeyIdTest, ImplicitConversionToBase) {
  UnexportableAttestationKeyId derived_id;
  UnexportableSigningKeyId base_id = derived_id;
  EXPECT_EQ(derived_id, base_id);
}

TEST(UnexportableKeyIdTest, InequalityOfDifferentKeys) {
  UnexportableSigningKeyId signing_key_id;
  UnexportableAttestationKeyId attestation_key_id;
  EXPECT_NE(signing_key_id, attestation_key_id);
}

TEST(UnexportableKeyIdTest, EqualityOfSameKeys) {
  UnexportableSigningKeyId base_id;
  UnexportableSigningKeyId signing_key_id(base_id);
  UnexportableAttestationKeyId attestation_key_id(base_id);
  EXPECT_EQ(signing_key_id, attestation_key_id);
}

TEST(UnexportableKeyIdTest, HashMapIntegration) {
  UnexportableSigningKeyId base_id;
  UnexportableSigningKeyId signing_key_id(base_id);
  UnexportableAttestationKeyId attestation_key_id(base_id);

  absl::flat_hash_map<UnexportableSigningKeyId, int> map;
  map[signing_key_id] = 1;
  map[attestation_key_id] = 2;

  // Since they wrap the same base value, they should be considered the same
  // key.
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map[signing_key_id], 2);
}

}  // namespace unexportable_keys
