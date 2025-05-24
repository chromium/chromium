// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_factory_views.h"

#include <memory>
#include <utility>

#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_event_relay.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_views.h"
#include "components/user_education/views/toggle_tracked_element_attention_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace user_education {

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleFactoryViews)

HelpBubbleFactoryViews::HelpBubbleFactoryViews(
    const HelpBubbleDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

HelpBubbleFactoryViews::~HelpBubbleFactoryViews() = default;

std::unique_ptr<HelpBubble> HelpBubbleFactoryViews::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  internal::HelpBubbleAnchorParams anchor;
  anchor.view = element->AsA<views::TrackedElementViews>()->view();
  std::unique_ptr<HelpBubbleEventRelay> event_relay;
  if (auto* menu_item = views::AsViewClass<views::MenuItemView>(anchor.view)) {
    event_relay =
        std::make_unique<internal::MenuHelpBubbleEventProcessor>(menu_item);
  }
  return CreateBubbleImpl(element, anchor, std::move(params),
                          std::move(event_relay));
}

bool HelpBubbleFactoryViews::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return element->IsA<views::TrackedElementViews>();
}

std::unique_ptr<HelpBubble> HelpBubbleFactoryViews::CreateBubbleImpl(
    ui::TrackedElement* element,
    const internal::HelpBubbleAnchorParams& anchor,
    HelpBubbleParams params,
    std::unique_ptr<HelpBubbleEventRelay> event_relay) {
  anchor.view->SetProperty(kHasInProductHelpPromoKey, true);
  auto result = base::WrapUnique(new HelpBubbleViews(
      new HelpBubbleView(delegate_, anchor, std::move(params),
                         std::move(event_relay)),
      element));
  for (const auto& accelerator :
       delegate_->GetPaneNavigationAccelerators(element)) {
    result->help_bubble_view_->GetFocusManager()->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::HandlerPriority::kNormalPriority,
        result.get());
  }
  if (result) {
    MaybeApplyAttentionStateToTrackedElement(anchor.view);
  }
  return result;
}

}  // namespace user_education
