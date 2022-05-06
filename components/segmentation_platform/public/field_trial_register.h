// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FIELD_TRIAL_REGISTER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FIELD_TRIAL_REGISTER_H_

#include "base/strings/string_piece.h"

namespace segmentation_platform {

// A delegate class that handles recording of the selected segmentation groups
// to metrics.
class FieldTrialRegister {
 public:
  FieldTrialRegister() = default;
  virtual ~FieldTrialRegister() = default;

  FieldTrialRegister(FieldTrialRegister&) = delete;
  FieldTrialRegister& operator=(FieldTrialRegister&) = delete;

  // Records that the current session uses `trial_name` and `group_name` as
  // segmentation groups. Calling multiple times with same `trial_name`
  // will replace the existing group with the new one, but note that the
  // previous logs already closed / staged for upload will not be changed.
  virtual void RegisterFieldTrial(base::StringPiece trial_name,
                                  base::StringPiece group_name) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FIELD_TRIAL_REGISTER_H_
