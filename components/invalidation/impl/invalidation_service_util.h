// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SERVICE_UTIL_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SERVICE_UTIL_H_

#include <string>

namespace invalidation {

// Generates a unique client ID for the invalidator.
std::string GenerateInvalidatorClientId();

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SERVICE_UTIL_H_
