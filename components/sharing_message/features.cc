// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/features.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/sync_preferences/features.h"

BASE_FEATURE(kOneTimeTokenBackendNotification,
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
