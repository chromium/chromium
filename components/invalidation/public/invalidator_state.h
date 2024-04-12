// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATOR_STATE_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATOR_STATE_H_

#include <string_view>

#include "components/invalidation/public/invalidation_export.h"

namespace invalidation {

enum class InvalidatorState : bool { kDisabled, kEnabled };

INVALIDATION_EXPORT std::string_view InvalidatorStateToString(
    InvalidatorState state);

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATOR_STATE_H_
