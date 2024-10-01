// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_FEATURES_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace affiliations::features {

COMPONENT_EXPORT(AFFILIATION_FEATURES)
BASE_DECLARE_FEATURE(kAffiliationsGroupInfoEnabled);

}  // namespace affiliations::features

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_FEATURES_H_
