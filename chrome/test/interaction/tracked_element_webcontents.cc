
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/tracked_element_webcontents.h"

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

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedElementWebContents)
