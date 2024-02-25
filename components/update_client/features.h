// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_FEATURES_H_
#define COMPONENTS_UPDATE_CLIENT_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace update_client::features {
BASE_DECLARE_FEATURE(kPuffinPatches);

#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kBackgroundCrxDownloaderMac);
#endif
}  // namespace update_client::features

#endif  // COMPONENTS_UPDATE_CLIENT_FEATURES_H_
