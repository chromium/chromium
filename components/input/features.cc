// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/features.h"

namespace input::features {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kInputOnViz, "InputOnViz", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace input::features
