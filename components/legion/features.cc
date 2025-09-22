// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/features.h"

namespace legion {

BASE_FEATURE(kLegion, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kLegionApiKey{&kLegion, "api-key", ""};

const base::FeatureParam<std::string> kLegionUrl{&kLegion, "url", ""};

}  // namespace legion
