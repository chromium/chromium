// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/devtools_interaction_tracker.h"

#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"

namespace subresource_filter {

DevtoolsInteractionTracker::DevtoolsInteractionTracker(
    content::WebContents* web_contents)
    : content::WebContentsUserData<DevtoolsInteractionTracker>(*web_contents) {}

DevtoolsInteractionTracker::~DevtoolsInteractionTracker() = default;

void DevtoolsInteractionTracker::ToggleForceActivation(bool force_activation) {
  if (!activated_via_devtools_ && force_activation)
    ContentSubresourceFilterThrottleManager::LogAction(
        SubresourceFilterAction::kForcedActivationEnabled);
  activated_via_devtools_ = force_activation;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DevtoolsInteractionTracker);

}  // namespace subresource_filter
