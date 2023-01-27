// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/features.h"

#include "build/build_config.h"

namespace download {

BASE_FEATURE(kDownloadServiceFeature,
             "DownloadService",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadServiceForegroundSessionIOSFeature,
             "DownloadServiceForegroundSessionIOSFeature",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

}  // namespace download
