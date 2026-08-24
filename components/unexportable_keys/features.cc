// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/features.h"

#include "build/build_config.h"

namespace unexportable_keys {

BASE_FEATURE(kEnableBoundSessionCredentialsSoftwareKeysForManualTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUnexportableKeyDeletion,
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnableUnexportableKeysSpareKeyPool,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace unexportable_keys
