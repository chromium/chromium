// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/features.h"

namespace data_controls {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableClipboardDataControlsAndroid,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kEnableDownloadDataControls, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace data_controls
