// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TRIGGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TRIGGER_H_

#include "base/types/id_type.h"

namespace segmentation_platform {

// ID for identifying a specific training data.
// TODO(haileywang): Consider evolving this into a struct that has a request
// type and ID.
using TrainingRequestId = base::IdType64<class RequestIdTag>;

// ID for identifying a specific feature processor state.
using FeatureProcessorStateId = base::IdType64<class RequestIdTag>;

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TRIGGER_H_
