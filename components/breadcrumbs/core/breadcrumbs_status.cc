// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumbs_status.h"

#include "base/feature_list.h"
#include "components/breadcrumbs/core/features.h"

namespace breadcrumbs {

bool IsEnabled() {
  return base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs);
}

}  // namespace breadcrumbs
