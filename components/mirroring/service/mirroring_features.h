// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRRORING_FEATURES_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRRORING_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace mirroring {
namespace features {

COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::Feature kOpenscreenCastStreamingSession;

COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::Feature kCastStreamingAv1;

COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::Feature kCastStreamingVp9;

COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::Feature kCastUseBlocklistForRemotingQuery;

COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::Feature kCastForceEnableRemotingQuery;

bool IsCastStreamingAV1Enabled();

}  // namespace features
}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRRORING_FEATURES_H_
