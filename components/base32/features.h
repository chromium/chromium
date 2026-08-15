// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BASE32_FEATURES_H_
#define COMPONENTS_BASE32_FEATURES_H_

#include "base/component_export.h"
#include "base/feature.h"

namespace base32::features {

// Enable the Rust implementation of the base32 encoder/decoder.
// TODO(crbug.com/536936880): Migrate to Rust implementation.
COMPONENT_EXPORT(BASE32_FEATURES) BASE_DECLARE_FEATURE(kComponentsBase32InRust);

}  // namespace base32::features

#endif  // COMPONENTS_BASE32_FEATURES_H_
