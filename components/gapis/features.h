// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAPIS_FEATURES_H_
#define COMPONENTS_GAPIS_FEATURES_H_

#include "base/feature_list.h"

namespace gapis {

// When enabled, the Gapis service will be initialized and request app token on
// profile creation.
BASE_DECLARE_FEATURE(kEnableGapis);

}  // namespace gapis

#endif  // COMPONENTS_GAPIS_FEATURES_H_
