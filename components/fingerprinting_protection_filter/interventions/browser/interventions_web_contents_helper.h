// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_BROWSER_INTERVENTIONS_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_BROWSER_INTERVENTIONS_WEB_CONTENTS_HELPER_H_

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace fingerprinting_protection_interventions {

// TODO(https://crbug.com/377325952): Remove InterventionsWebContentsHelper as
// it's no longer needed.

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

 private:
  friend class content::WebContentsUserData<InterventionsWebContentsHelper>;
  bool is_incognito_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace fingerprinting_protection_interventions

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_BROWSER_INTERVENTIONS_WEB_CONTENTS_HELPER_H_
