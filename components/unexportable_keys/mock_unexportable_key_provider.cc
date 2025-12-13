// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/mock_unexportable_key_provider.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace unexportable_keys {

using ::testing::Return;

MockUnexportableKeyProvider::MockUnexportableKeyProvider() {
  ON_CALL(*this, AsStatefulUnexportableKeyProvider).WillByDefault(Return(this));
}
MockUnexportableKeyProvider::~MockUnexportableKeyProvider() = default;

}  // namespace unexportable_keys
