// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_ANIMATION_UTILS_H_
#define CHROME_BROWSER_VR_TEST_ANIMATION_UTILS_H_

#include "chrome/browser/vr/target_property.h"

#include "ui/gfx/animation/keyframe/test/animation_utils.h"

namespace vr {

class UiElement;

// Returns true if the given properties are being animated by the element.
bool IsAnimating(UiElement* element,
                 const std::vector<TargetProperty>& properties);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_ANIMATION_UTILS_H_
