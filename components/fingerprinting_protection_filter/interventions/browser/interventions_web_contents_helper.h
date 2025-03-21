// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_BROWSER_INTERVENTIONS_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_BROWSER_INTERVENTIONS_WEB_CONTENTS_HELPER_H_

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace fingerprinting_protection_interventions {

// The InterventionsWebContentsHelper facilitates browser-side decisions such as
// propagating and determining blink::RuntimeFeature enabled state overrides to
// subsequent navigations. This is used primarily to ensure Navigations receive
// the correct enablement state of the RuntimeFeature, with regard to other
// factors such as URL-level exceptions and incognito.
class InterventionsWebContentsHelper
    : public content::WebContentsUserData<InterventionsWebContentsHelper>,
      public content::WebContentsObserver {
 public:
  // TODO(https://crbug.com/380458351): Add incognito bool upon creation of this
  // WebContentsHelper.
  static void CreateForWebContents(content::WebContents* web_contents,
                                   bool is_incognito);

  InterventionsWebContentsHelper(const InterventionsWebContentsHelper&) =
      delete;
  InterventionsWebContentsHelper& operator=(
      const InterventionsWebContentsHelper&) = delete;

  ~InterventionsWebContentsHelper() override;

 protected:
  InterventionsWebContentsHelper(content::WebContents* web_contents,
                                 bool is_incognito);

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<InterventionsWebContentsHelper>;
  bool is_incognito_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace fingerprinting_protection_interventions

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_BROWSER_INTERVENTIONS_WEB_CONTENTS_HELPER_H_
