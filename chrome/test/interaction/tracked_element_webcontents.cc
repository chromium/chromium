
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/tracked_element_webcontents.h"

#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"

DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(TrackedElementWebContents,
                                       kFirstNonEmptyPaint);

TrackedElementWebContents::TrackedElementWebContents(
    ui::ElementIdentifier identifier,
    ui::ElementContext context,
    WebContentsInteractionTestUtil* owner)
    : TrackedElement(identifier, context), owner_(owner) {}

TrackedElementWebContents::~TrackedElementWebContents() {
  ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(this);
}

void TrackedElementWebContents::Init() {
  ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(this);
}

gfx::Rect TrackedElementWebContents::GetScreenBounds() const {
  return owner_->web_contents()->GetContainerBounds();
}

std::string TrackedElementWebContents::ToString() const {
  auto result = TrackedElement::ToString();
  result.append(" with contents ");
  result.append(owner_->web_contents()->GetURL().spec());
  return result;
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedElementWebContents)
