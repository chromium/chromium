// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/features.h"

namespace download {

BASE_FEATURE(kDownloadServiceFeature,
             "DownloadService",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadServiceForegroundSessionIOSFeature,
             "DownloadServiceForegroundSessionIOSFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace download
