// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "base/feature_list.h"

namespace power_bookmarks {

BASE_FEATURE(kPowerBookmarkBackend,
             "PowerBookmarkBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace power_bookmarks
