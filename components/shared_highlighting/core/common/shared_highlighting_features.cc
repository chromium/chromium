// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_features.h"

#include "base/feature_list.h"

namespace shared_highlighting {

const base::Feature kPreemptiveLinkToTextGeneration{
    "PreemptiveLinkToTextGeneration", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharedHighlightingUseBlocklist{
    "SharedHighlightingUseBlocklist", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kSharedHighlightingV2{"SharedHighlightingV2",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace shared_highlighting
