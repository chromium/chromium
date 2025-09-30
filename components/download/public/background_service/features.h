// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_FEATURES_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace download {

// Used to set configuration of download service through Finch. This is not used
// to turn on/off the feature.
COMPONENT_EXPORT(COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE)
BASE_DECLARE_FEATURE(kDownloadServiceFeature);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_FEATURES_H_
