// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_FACTORY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_FACTORY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace ui {
class TrackedElement;
}

namespace user_education {

class HelpBubble;
struct HelpBubbleParams;

// HelpBubbleFactory is an interface for opening and closing bubbles from a
// running user education journey. Since some platforms/implementations of
// bubbles will have their own reqirements/constraints for if/when/how to show
// a bubble, we only call into the implementation asking it to show or hide the
// bubble. All other state must be maintained by the caller.
class HelpBubbleFactory : public ui::FrameworkSpecificImplementation {
 public:
  HelpBubbleFactory() = default;
  ~HelpBubbleFactory() override = default;

  // Returns whether the bubble owner can show a bubble for the TrackedElement.
  virtual bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const = 0;

  // Called to actually show the bubble.
  virtual std::unique_ptr<HelpBubble> CreateBubble(ui::TrackedElement* element,
                                                   HelpBubbleParams params) = 0;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_FACTORY_H_
