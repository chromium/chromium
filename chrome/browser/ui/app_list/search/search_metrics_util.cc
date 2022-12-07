// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace app_list {

void LogError(Error error) {
  base::UmaHistogramEnumeration(base::StrCat({kHistogramPrefix, "Error"}),
                                error);
}

}  // namespace app_list
