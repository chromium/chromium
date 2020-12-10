// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/profile_interaction_manager.h"

#include "base/logging.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

namespace subresource_filter {

ProfileInteractionManager::ProfileInteractionManager(
    content::WebContents* web_contents,
    SubresourceFilterProfileContext* profile_context)
    : content::WebContentsObserver(web_contents),
      profile_context_(profile_context) {
  DCHECK(web_contents);
}

ProfileInteractionManager::~ProfileInteractionManager() = default;

void ProfileInteractionManager::OnReloadRequested() {
  ContentSubresourceFilterThrottleManager::LogAction(
      SubresourceFilterAction::kAllowlistedSite);
  profile_context_->settings_manager()->AllowlistSite(
      web_contents()->GetLastCommittedURL());

  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

}  // namespace subresource_filter
