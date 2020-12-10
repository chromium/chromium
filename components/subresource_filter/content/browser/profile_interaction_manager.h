// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PROFILE_INTERACTION_MANAGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PROFILE_INTERACTION_MANAGER_H_

#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace subresource_filter {

class SubresourceFilterProfileContext;

// Class that manages interaction between interaction between the
// per-navigation/per-tab subresource filter objects (i.e., the throttles and
// throttle manager) and the per-profile objects (e.g., content settings).
class ProfileInteractionManager : public content::WebContentsObserver {
 public:
  ProfileInteractionManager(content::WebContents* web_contents,
                            SubresourceFilterProfileContext* profile_context);
  ~ProfileInteractionManager() override;

  ProfileInteractionManager(const ProfileInteractionManager&) = delete;
  ProfileInteractionManager& operator=(const ProfileInteractionManager&) =
      delete;

  // Invoked when the user has requested a reload of a page with blocked ads
  // (e.g., via an infobar).
  void OnReloadRequested();

 private:
  // Unowned and must outlive this object.
  SubresourceFilterProfileContext* profile_context_ = nullptr;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PROFILE_INTERACTION_MANAGER_H_
