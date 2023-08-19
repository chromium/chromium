// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace update_client::features {
BASE_FEATURE(kPuffinPatches, "PuffinPatches", base::FEATURE_ENABLED_BY_DEFAULT);
}
