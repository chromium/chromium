// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/tracked_element_webui.h"

#include "base/check.h"
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
