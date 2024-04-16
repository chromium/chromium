// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRRORING_FEATURES_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRRORING_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace mirroring {
namespace features {

// TODO(crbug.com/1363512): Remove support for sender side letterboxing.
COMPONENT_EXPORT(MIRRORING_SERVICE)
BASE_DECLARE_FEATURE(kCastDisableLetterboxing);

// TODO(crbug.com/40177436): Remove model name checks for querying receiver
// capabilities.
COMPONENT_EXPORT(MIRRORING_SERVICE)
BASE_DECLARE_FEATURE(kCastDisableModelNameCheck);

// TODO(crbug.com/40255351): Should be removed once working properly.
COMPONENT_EXPORT(MIRRORING_SERVICE)
BASE_DECLARE_FEATURE(kCastEnableStreamingWithHiDPI);

}  // namespace features
}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRRORING_FEATURES_H_
