// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_FEATURES_H_
#define COMPONENTS_EMBEDDER_SUPPORT_FEATURES_H_

#include "base/feature_list.h"

namespace embedder_support {

// Sets "XR" in the form factor filed of client hints.
BASE_DECLARE_FEATURE(kClientHintsXRFormFactor);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_FEATURES_H_
