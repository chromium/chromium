// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STRIKE_DATABASE_STRIKE_DATABASE_FEATURES_H_
#define COMPONENTS_STRIKE_DATABASE_STRIKE_DATABASE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace strike_database::features {

COMPONENT_EXPORT(STRIKE_DATABASE) BASE_DECLARE_FEATURE(kDisableStrikeSystem);

}  // namespace strike_database::features

#endif  // COMPONENTS_STRIKE_DATABASE_STRIKE_DATABASE_FEATURES_H_
