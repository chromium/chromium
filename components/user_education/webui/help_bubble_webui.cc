// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/help_bubble_webui.h"

#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace user_education {

HelpBubbleWebUI::HelpBubbleWebUI(HelpBubbleHandlerBase* handler)
    : handler_(handler) {
  CHECK(handler_);
}
HelpBubbleWebUI::~HelpBubbleWebUI() {
  Close();
}

bool HelpBubbleWebUI::ToggleFocusForAccessibility() {
  return handler_->ToggleHelpBubbleFocusForAccessibility();
}

gfx::Rect HelpBubbleWebUI::GetBoundsInScreen() const {
  return handler_->GetHelpBubbleBoundsInScreen();
}

ui::ElementContext HelpBubbleWebUI::GetContext() const {
  return handler_->context();
}

void HelpBubbleWebUI::CloseBubbleImpl() {
  handler_->OnHelpBubbleClosing();
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleWebUI)

HelpBubbleFactoryWebUI::HelpBubbleFactoryWebUI() = default;
HelpBubbleFactoryWebUI::~HelpBubbleFactoryWebUI() = default;

std::unique_ptr<HelpBubble> HelpBubbleFactoryWebUI::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  HelpBubbleHandlerBase* const handler =
      element->AsA<TrackedElementWebUI>()->handler();
  return handler->CreateHelpBubble(std::move(params));
}

bool HelpBubbleFactoryWebUI::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return element->IsA<TrackedElementWebUI>();
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleFactoryWebUI)

}  // namespace user_education
