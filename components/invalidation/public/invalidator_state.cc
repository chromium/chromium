// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidator_state.h"

#include "base/notreached.h"

namespace invalidation {

std::string_view InvalidatorStateToString(InvalidatorState state) {
  switch (state) {
    case InvalidatorState::kEnabled:
      return "Enabled";
    case InvalidatorState::kDisabled:
      return "Disabled";
  }
  NOTREACHED();
}

}  // namespace invalidation
