// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/features/chromeos_features.h"

namespace chromeos {

// Feature flag for disable/enable Lacros TTS support.
// Disable by default before the feature is completedly implemented.
const base::Feature kLacrosTtsSupport{"LacrosTtsSupport",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace chromeos
