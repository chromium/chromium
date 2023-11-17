// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/features.h"

#include "base/feature_list.h"

namespace embedder_support {

BASE_FEATURE(kClientHintsXRFormFactor,
             "ClientHintsXRFormFactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace embedder_support
