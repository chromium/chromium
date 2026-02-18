// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_SITE_SEARCH_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_SITE_SEARCH_H_

#include <string>

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"

class OmniboxActionSiteSearch : public OmniboxAction {
 public:
  explicit OmniboxActionSiteSearch(const TemplateURL* template_url);

  // OmniboxAction overrides:
  OmniboxActionId ActionId() const override;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const override;
#endif

 private:
  ~OmniboxActionSiteSearch() override;

  const std::u16string keyword_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_SITE_SEARCH_H_
