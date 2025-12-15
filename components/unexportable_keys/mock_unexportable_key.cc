// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/mock_unexportable_key.h"

namespace unexportable_keys {

MockUnexportableKey::MockUnexportableKey() {
  ON_CALL(*this, AsStatefulUnexportableSigningKey())
      .WillByDefault(testing::Return(this));
}
MockUnexportableKey::~MockUnexportableKey() = default;

}  // namespace unexportable_keys
