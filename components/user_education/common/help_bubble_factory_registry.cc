// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/help_bubble_factory_registry.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace user_education {

HelpBubbleFactoryRegistry::HelpBubbleFactoryRegistry() = default;

HelpBubbleFactoryRegistry::~HelpBubbleFactoryRegistry() {
  for (auto& pr : help_bubbles_) {
    // Unsubscribe from the bubble before trying to close it so we don't try to
    // modify the map while we're iterating it.
    pr.second = base::CallbackListSubscription();
    pr.first->Close();
  }
}

std::unique_ptr<HelpBubble> HelpBubbleFactoryRegistry::CreateHelpBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  CHECK(element);
  for (auto& bubble_factory : factories_) {
    if (bubble_factory.CanBuildBubbleForTrackedElement(element)) {
      auto result = bubble_factory.CreateBubble(element, std::move(params));
      if (result) {
        help_bubbles_.emplace(
            result.get(), result->AddOnCloseCallback(base::BindOnce(
                              &HelpBubbleFactoryRegistry::OnHelpBubbleClosed,
                              base::Unretained(this))));
      }
      return result;
    }
  }
  return nullptr;
}

void HelpBubbleFactoryRegistry::NotifyAnchorBoundsChanged(
    ui::ElementContext context) {
  for (const auto& pr : help_bubbles_) {
    if (pr.first->GetContext() == context)
      pr.first->OnAnchorBoundsChanged();
  }
}

bool HelpBubbleFactoryRegistry::ToggleFocusForAccessibility(
    ui::ElementContext context) {
  for (const auto& pr : help_bubbles_) {
    if (pr.first->GetContext() == context &&
        pr.first->ToggleFocusForAccessibility()) {
      return true;
    }
  }
  return false;
}

HelpBubble* HelpBubbleFactoryRegistry::GetHelpBubble(
    ui::ElementContext context) {
  for (const auto& pr : help_bubbles_) {
    if (pr.first->GetContext() == context)
      return pr.first;
  }
  return nullptr;
}

void HelpBubbleFactoryRegistry::OnHelpBubbleClosed(HelpBubble* bubble,
                                                   HelpBubble::CloseReason) {
  const auto result = help_bubbles_.erase(bubble);
  DCHECK(result);
}

}  // namespace user_education
