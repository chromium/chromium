// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/features.h"
#include "base/feature_list.h"

namespace plus_addresses {
// Controls the enabled/disabled state of the experimental feature.
BASE_FEATURE(kFeature,
             "PlusAddressesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace plus_addresses
