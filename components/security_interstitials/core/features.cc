// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace security_interstitials::features {

// Enables a dialog-based UI for HTTPS-First Mode.
// The flag is currently disabled on Android as the new UI is not implemented
// there yet. (See crbug.com/469092867 for some previous unintended side
// effects.)
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kHttpsFirstDialogUi, base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kHttpsFirstDialogUi, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace security_interstitials::features
