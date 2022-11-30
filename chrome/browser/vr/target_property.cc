// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/target_property.h"

#include "ui/gfx/animation/keyframe/target_property.h"

namespace vr {

static_assert(TargetProperty::NUM_TARGET_PROPERTIES - 1 <
                  gfx::kMaxTargetPropertyIndex,
              "The number of vr target properties has exceeded the capacity of"
              " TargetProperties");

}  // namespace vr
