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

TEST(UnexportableSigningKeyIdTest, ExplicitConversionFromBase) {
  static_assert(
      !std::convertible_to<UnexportableKeyId, UnexportableSigningKeyId>,
      "Implicit conversion from UnexportableKeyId to "
      "UnexportableSigningKeyId should not be allowed");

  UnexportableKeyId base_id;
  UnexportableSigningKeyId derived_id(base_id);
  EXPECT_EQ(derived_id, base_id);
}

TEST(UnexportableSigningKeyIdTest, ImplicitConversionToBase) {
  UnexportableSigningKeyId derived_id;
  UnexportableKeyId base_id = derived_id;
  EXPECT_EQ(derived_id, base_id);
}

TEST(UnexportableAttestationKeyIdTest, DefaultConstructor) {
  UnexportableAttestationKeyId attestation_key_id;
  EXPECT_FALSE(attestation_key_id.value().is_empty());
}

TEST(UnexportableAttestationKeyIdTest, ExplicitConversionFromBase) {
  static_assert(
      !std::convertible_to<UnexportableKeyId, UnexportableAttestationKeyId>,
      "Implicit conversion from UnexportableKeyId to "
      "UnexportableAttestationKeyId should not be allowed");

  UnexportableKeyId base_id;
  UnexportableAttestationKeyId derived_id(base_id);
  EXPECT_EQ(derived_id, base_id);
}

TEST(UnexportableAttestationKeyIdTest, ImplicitConversionToBase) {
  UnexportableAttestationKeyId derived_id;
  UnexportableKeyId base_id = derived_id;
  EXPECT_EQ(derived_id, base_id);
}

TEST(UnexportableKeyIdTest, InequalityOfDifferentKeys) {
  UnexportableSigningKeyId signing_key_id;
  UnexportableAttestationKeyId attestation_key_id;
  EXPECT_NE(signing_key_id, attestation_key_id);
}

TEST(UnexportableKeyIdTest, EqualityOfSameKeys) {
  UnexportableKeyId base_id;
  UnexportableSigningKeyId signing_key_id(base_id);
  UnexportableAttestationKeyId attestation_key_id(base_id);
  EXPECT_EQ(signing_key_id, attestation_key_id);
}

TEST(UnexportableKeyIdTest, HashMapIntegration) {
  UnexportableKeyId base_id;
  UnexportableSigningKeyId signing_key_id(base_id);
  UnexportableAttestationKeyId attestation_key_id(base_id);

  absl::flat_hash_map<UnexportableKeyId, int> map;
  map[signing_key_id] = 1;
  map[attestation_key_id] = 2;

  // Since they wrap the same base value, they should be considered the same
  // key.
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map[signing_key_id], 2);
}

}  // namespace unexportable_keys
