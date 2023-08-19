// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_FEATURE_REFS_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_FEATURE_REFS_H_

#include <vector>

#include "base/component_export.h"
#include "base/test/scoped_feature_list.h"

namespace ash::standalone_browser {

// Get the set of features that tests should enable in order to turn on Lacros.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
const std::vector<base::test::FeatureRef>& GetFeatureRefs();

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_FEATURE_REFS_H_
