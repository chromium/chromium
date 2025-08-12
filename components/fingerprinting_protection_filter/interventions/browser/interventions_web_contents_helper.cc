// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/interventions/browser/interventions_web_contents_helper.h"

#include "base/feature_list.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace fingerprinting_protection_interventions {

// TODO(https://crbug.com/377325952): Remove InterventionsWebContentsHelper.
// User bypass will now be set in ContentBrowserClient, as such, this class is
// no longer needed.

// static
void InterventionsWebContentsHelper::CreateForWebContents(
    content::WebContents* web_contents,
    bool is_incognito) {
  // Do nothing if a InterventionsWebContentsHelper
  // already exists for the current WebContents.
  if (FromWebContents(web_contents)) {
    return;
  }

  content::WebContentsUserData<
      InterventionsWebContentsHelper>::CreateForWebContents(web_contents,
                                                            is_incognito);
}

// private
InterventionsWebContentsHelper::InterventionsWebContentsHelper(
    content::WebContents* web_contents,
    bool is_incognito)
    : content::WebContentsUserData<InterventionsWebContentsHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      is_incognito_(is_incognito) {}

InterventionsWebContentsHelper::~InterventionsWebContentsHelper() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(InterventionsWebContentsHelper);

}  // namespace fingerprinting_protection_interventions
