// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_action_site_search.h"

#include "build/build_config.h"
#include "components/search_engines/template_url.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/omnibox/browser/actions/omnibox_action_factory_android.h"
#endif

OmniboxActionSiteSearch::OmniboxActionSiteSearch(
    const TemplateURL* template_url)
    : OmniboxAction(OmniboxAction::LabelStrings(template_url->GetFullName(),
                                                template_url->GetFullName(),
                                                u"",
                                                template_url->GetFullName()),
                    {}),
      keyword_(template_url->keyword()) {}

OmniboxActionSiteSearch::~OmniboxActionSiteSearch() = default;

OmniboxActionId OmniboxActionSiteSearch::ActionId() const {
  return OmniboxActionId::SITE_SEARCH;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
OmniboxActionSiteSearch::GetOrCreateJavaObject(JNIEnv* env) const {
  if (!j_omnibox_action_) {
    j_omnibox_action_.Reset(BuildSiteSearchAction(
        env, reinterpret_cast<intptr_t>(this), strings_.hint,
        strings_.accessibility_hint, keyword_));
  }
  return base::android::ScopedJavaLocalRef<jobject>(j_omnibox_action_);
}
#endif
