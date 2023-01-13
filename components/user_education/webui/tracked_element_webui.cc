// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/tracked_element_webui.h"

#include "base/check.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace user_education {

TrackedElementWebUI::TrackedElementWebUI(HelpBubbleHandlerBase* handler,
                                         ui::ElementIdentifier identifier,
                                         ui::ElementContext context)
    : TrackedElement(identifier, context), handler_(handler) {
  DCHECK(handler);
}

TrackedElementWebUI::~TrackedElementWebUI() {
  SetVisible(false);
}

gfx::Rect TrackedElementWebUI::GetScreenBounds() const {
  gfx::Rect result;
  content::WebContents* const contents =
      handler_->GetController()->web_ui()->GetWebContents();
  if (contents) {
    // TODO(dfried): this is a placeholder; the actual bounds of the element in
    // the view should be offset by the origin of this rectangle.
    result = contents->GetContainerBounds();
  }
  return result;
}

void TrackedElementWebUI::SetVisible(bool visible) {
  if (visible == visible_)
    return;

  visible_ = visible;
  auto* const delegate = ui::ElementTracker::GetFrameworkDelegate();
  if (visible) {
    delegate->NotifyElementShown(this);
  } else {
    delegate->NotifyElementHidden(this);
  }
}

void TrackedElementWebUI::Activate() {
  DCHECK(visible_);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(this);
}

void TrackedElementWebUI::CustomEvent(ui::CustomElementEventType event_type) {
  DCHECK(visible_);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(this,
                                                                event_type);
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedElementWebUI)

}  // namespace user_education
