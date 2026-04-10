// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_id.h"

#include <concepts>

#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace unexportable_keys
