// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_SCOPED_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_SCOPED_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_

#include <memory>

#include "base/containers/queue.h"
#include "components/unexportable_keys/mock_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

// Causes `GetUnexportableKeyProvider()` to return fully mockable
// `MockUnexportableKey`s while it is in scope.
// The mock provider will return mock keys previously added via
// `AddNextGeneratedKey()` in the queue order.
class ScopedMockUnexportableKeyProvider {
 public:
  ScopedMockUnexportableKeyProvider();
  ScopedMockUnexportableKeyProvider(const ScopedMockUnexportableKeyProvider&) =
      delete;
  ScopedMockUnexportableKeyProvider& operator=(
      const ScopedMockUnexportableKeyProvider&) = delete;
  ~ScopedMockUnexportableKeyProvider();

  MockUnexportableKeyProvider& mock() { return mock_provider_; }

  void AddNextGeneratedKey(std::unique_ptr<crypto::UnexportableSigningKey> key);

 private:
  MockUnexportableKeyProvider mock_provider_;
  base::queue<std::unique_ptr<crypto::UnexportableSigningKey>>
      next_generated_keys_;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_SCOPED_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
