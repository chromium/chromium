// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_factory_mac.h"

#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

namespace user_education {

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleFactoryMac)

HelpBubbleFactoryMac::HelpBubbleFactoryMac(const HelpBubbleDelegate* delegate)
    : delegate_(delegate) {}
HelpBubbleFactoryMac::~HelpBubbleFactoryMac() = default;

std::unique_ptr<HelpBubble> HelpBubbleFactoryMac::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  auto* const element_mac = element->AsA<ui::TrackedElementMac>();
  views::Widget* const widget =
      views::ElementTrackerViews::GetInstance()->GetWidgetForContext(
          element_mac->context());

  // Because the exact location of the menu item cannot be determined, an arrow
  // is not shown on the bubble.
  internal::HelpBubbleAnchorParams anchor;
  anchor.view = widget->GetRootView();
  anchor.rect = element_mac->GetScreenBounds();

  if (@available(macOS 14.0, *)) {
    // In MacOS 14.0 and later, the screen bounds reported by the element are
    // the exact bounds of the menu item, which means that bubbles should be
    // placed accurately and do not need additional visual adjustment.
  } else {
    // In earlier versions of MacOS, individual menu item bounds aren't
    // available, and the entire menu's bounds are returned. Because of this,
    // bubbles need to float next to the menu rather than attach to a particular
    // menu item. Don't render an arrow and leave some space from the side of
    // the menu to ensure that the bubble appears associated with the menu but
    // not any one specific item.
    anchor.show_arrow = false;
    constexpr auto kMacMenuInsets = gfx::Insets::VH(10, -5);
    anchor.rect->Inset(kMacMenuInsets);
  }

  return base::WrapUnique(new HelpBubbleViews(
      new HelpBubbleView(delegate_, anchor, std::move(params)), element));
}

bool HelpBubbleFactoryMac::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return element->IsA<ui::TrackedElementMac>();
}

}  // namespace user_education
