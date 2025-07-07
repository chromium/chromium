// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/features.h"

namespace data_controls {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableClipboardDataControlsAndroid,
             "EnableClipboardDataControlsAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kEnableDownloadDataControlsDesktop,
             "EnableDownloadDataControlsDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace data_controls
