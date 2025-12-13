// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/strike_database/strike_database_features.h"

#include "base/feature_list.h"

namespace strike_database::features {

// If enabled, the strike system will not block features. Intended for
// debugging/testing use only and should never be launched to users.
BASE_FEATURE(kDisableStrikeSystem,
             "DisableAutofillStrikeSystem",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace strike_database::features
