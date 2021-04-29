// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/feature_tutorial_service.h"

#include "base/check.h"
#include "build/build_config.h"

FeatureTutorialService::FeatureTutorialService() = default;
FeatureTutorialService::~FeatureTutorialService() = default;

// For Views, this method is defined elsewhere. For non-Views it should never be
// called.
#if !defined(TOOLKIT_VIEWS)
// static
std::unique_ptr<FeatureTutorialService> FeatureTutorialService::MakeInstance(
    Profile* profile) {
  CHECK(0);
}
#endif  // !defined(TOOLKIT_VIEWS)
