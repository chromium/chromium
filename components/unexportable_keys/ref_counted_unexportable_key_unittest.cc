// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/ref_counted_unexportable_key.h"

#include <memory>

#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/mock_unexportable_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

TEST(RefCountedUnexportableKeyTest, RefCountedUnexportableSigningKey) {
  auto mock_key = std::make_unique<crypto::MockUnexportableSigningKey>();
  crypto::UnexportableSigningKey* mock_key_ptr = mock_key.get();
  UnexportableSigningKeyId id;

  auto ref_counted_key = base::MakeRefCounted<RefCountedUnexportableSigningKey>(
      std::move(mock_key), id);

  EXPECT_EQ(&ref_counted_key->key(), mock_key_ptr);
  EXPECT_EQ(ref_counted_key->id(), id);
}

TEST(RefCountedUnexportableKeyTest, RefCountedUnexportableAttestationKey) {
  auto mock_key = std::make_unique<crypto::MockUnexportableAttestationKey>();
  crypto::UnexportableAttestationKey* mock_key_ptr = mock_key.get();
  UnexportableAttestationKeyId id;

  auto ref_counted_key =
      base::MakeRefCounted<RefCountedUnexportableAttestationKey>(
          std::move(mock_key), id);

  EXPECT_EQ(&ref_counted_key->key(), mock_key_ptr);
  EXPECT_EQ(ref_counted_key->id(), id);
}

}  // namespace unexportable_keys
