// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/help_bubble_webui.h"

#include "components/user_education/webui/help_bubble_handler.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace user_education {

HelpBubbleWebUI::HelpBubbleWebUI(HelpBubbleHandlerBase* handler,
                                 ui::ElementIdentifier anchor_id)
    : handler_(handler), anchor_id_(anchor_id) {
  CHECK(handler_);
}

HelpBubbleWebUI::~HelpBubbleWebUI() {
  Close(HelpBubble::CloseReason::kBubbleDestroyed);
}

content::WebContents* HelpBubbleWebUI::GetWebContents() {
  return is_open() ? handler_->GetWebContents() : nullptr;
}

bool HelpBubbleWebUI::ToggleFocusForAccessibility() {
  return handler_->ToggleHelpBubbleFocusForAccessibility(anchor_id_);
}

gfx::Rect HelpBubbleWebUI::GetBoundsInScreen() const {
  return handler_->GetHelpBubbleBoundsInScreen(anchor_id_);
}

ui::ElementContext HelpBubbleWebUI::GetContext() const {
  return handler_->context();
}

bool HelpBubbleWebUI::Close(CloseReason reason) {
  auto on_close = BeginClose(reason);
  if (on_close.is_valid()) {
    handler_->OnHelpBubbleClosing(anchor_id_);
  }
  return on_close.is_valid();
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleWebUI)

HelpBubbleFactoryWebUI::HelpBubbleFactoryWebUI() = default;
HelpBubbleFactoryWebUI::~HelpBubbleFactoryWebUI() = default;

std::unique_ptr<HelpBubble> HelpBubbleFactoryWebUI::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  HelpBubbleHandlerBase* const handler =
      element->AsA<ui::TrackedElementWebUI>()->handler()->help_bubble_handler();
  return handler->CreateHelpBubble(element->identifier(), std::move(params));
}

bool HelpBubbleFactoryWebUI::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  if (const auto* element_webui = element->AsA<ui::TrackedElementWebUI>()) {
    return element_webui->handler()->help_bubble_handler() != nullptr;
  }
  return false;
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleFactoryWebUI)

}  // namespace user_education
