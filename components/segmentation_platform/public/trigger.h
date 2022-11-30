// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TRIGGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TRIGGER_H_

namespace segmentation_platform {

// Various trigger events that drive on-demand model execution.
enum class TriggerType {
  kNone = 0,
  kPageLoad = 1,
  kMaxValue = kPageLoad,
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TRIGGER_H_
