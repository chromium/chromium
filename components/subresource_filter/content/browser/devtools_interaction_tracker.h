// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_DEVTOOLS_INTERACTION_TRACKER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_DEVTOOLS_INTERACTION_TRACKER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace subresource_filter {

// Can be used to track whether forced activation has been set by devtools
// within a given WebContents.
// Scoped to the lifetime of a WebContents.
class DevtoolsInteractionTracker
    : public content::WebContentsUserData<DevtoolsInteractionTracker> {
 public:
  explicit DevtoolsInteractionTracker(content::WebContents* web_contents);
  ~DevtoolsInteractionTracker() override;

  DevtoolsInteractionTracker(const DevtoolsInteractionTracker&) = delete;
  DevtoolsInteractionTracker& operator=(const DevtoolsInteractionTracker&) =
      delete;

  // Should be called by devtools in response to a protocol command to enable ad
  // blocking in this WebContents. Should only persist while devtools is
  // attached.
  void ToggleForceActivation(bool force_activation);

  bool activated_via_devtools() { return activated_via_devtools_; }

 private:
  friend class content::WebContentsUserData<DevtoolsInteractionTracker>;

  // Corresponds to a devtools command which triggers filtering on all page
  // loads. We must be careful to ensure this boolean does not persist after the
  // devtools window is closed, which should be handled by the devtools system.
  bool activated_via_devtools_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_DEVTOOLS_INTERACTION_TRACKER_H_
