// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/features.h"

namespace signin {

const base::Feature kForceStartupSigninPromo{"ForceStartupSigninPromo",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

bool ForceStartupSigninPromo() {
  return base::FeatureList::IsEnabled(kForceStartupSigninPromo);
}

}  // namespace signin
