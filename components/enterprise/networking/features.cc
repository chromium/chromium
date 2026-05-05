// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/networking/features.h"

namespace enterprise {

BASE_FEATURE(kEnableProxyAuthenticationService,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsProxyAuthenticationEnabled() {
  return base::FeatureList::IsEnabled(kEnableProxyAuthenticationService);
}

}  // namespace enterprise
