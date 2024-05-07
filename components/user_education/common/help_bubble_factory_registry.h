// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_FACTORY_REGISTRY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_FACTORY_REGISTRY_H_

#include <map>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace user_education {

// HelpBubbleFactoryRegistry provides access to framework specific bubble
// factory implementations. HelpBubbleFactoryRegistry is not a strict singleton
// that instances can be created multiple times in a test environment.
class HelpBubbleFactoryRegistry {
 public:
  HelpBubbleFactoryRegistry();
  ~HelpBubbleFactoryRegistry();
  HelpBubbleFactoryRegistry(const HelpBubbleFactoryRegistry&) = delete;
  HelpBubbleFactoryRegistry& operator=(const HelpBubbleFactoryRegistry&) =
      delete;

  // Finds the appropriate factory and creates a bubble, or returns null if the
  // bubble cannot be created.
  std::unique_ptr<HelpBubble> CreateHelpBubble(ui::TrackedElement* element,
                                               HelpBubbleParams params);

  // Returns true if any bubble is showing in any framework.
  bool is_any_bubble_showing() const { return !help_bubbles_.empty(); }

  // Notifies frameworks that the anchor bounds for a bubble in the given
  // context might have changed. For frameworks which automatically reposition
  // bubbles, this will be a no-op.
  void NotifyAnchorBoundsChanged(ui::ElementContext context);

  // Sets focus on a help bubble if there is one in the given context (or
  // toggles between help) bubble and its anchor; returns false if there is no
  // bubble or nothing can be focused.
  bool ToggleFocusForAccessibility(ui::ElementContext context);

  // Gets the first visible help bubble in the given context, or null if none
  // exists.
  HelpBubble* GetHelpBubble(ui::ElementContext context);

  // Adds a bubble factory of type `T` to the list of bubble factories, if it
  // is not already present.
  template <class T, typename... Args>
  void MaybeRegister(Args&&... args) {
    factories_.MaybeRegister<T>(std::forward<Args>(args)...);
  }

 private:
  void OnHelpBubbleClosed(HelpBubble* help_bubble, HelpBubble::CloseReason);

  // The list of known factories.
  ui::FrameworkSpecificRegistrationList<HelpBubbleFactory> factories_;

  // The list of known help bubbles.
  std::map<HelpBubble*, base::CallbackListSubscription> help_bubbles_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_FACTORY_REGISTRY_H_
