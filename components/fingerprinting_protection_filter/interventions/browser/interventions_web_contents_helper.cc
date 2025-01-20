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
class NavigationHandle;
class WebContents;
}  // namespace content

namespace fingerprinting_protection_interventions {

// static
void InterventionsWebContentsHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  // Do nothing if a InterventionsWebContentsHelper
  // already exists for the current WebContents.
  if (FromWebContents(web_contents)) {
    return;
  }

  content::WebContentsUserData<
      InterventionsWebContentsHelper>::CreateForWebContents(web_contents);
}

// private
InterventionsWebContentsHelper::InterventionsWebContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<InterventionsWebContentsHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents) {}

InterventionsWebContentsHelper::~InterventionsWebContentsHelper() = default;

void InterventionsWebContentsHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(crbug.com/380461005): Add URL-level exceptions.
  // At the moment, this is just a sanity check to ensure the state of the
  // CanvasInterventions feature is the same in both blink and the browser.
  // Other factors, including URL-level exceptions can alter the state of the
  // CanvasInterventions feature.
  auto& mutable_runtime_feature_state =
      navigation_handle->GetMutableRuntimeFeatureStateContext();
  bool canvas_base_feature_enabled =
      features::IsCanvasInterventionsFeatureEnabled();

  if (mutable_runtime_feature_state.IsCanvasInterventionsEnabled() !=
      canvas_base_feature_enabled) {
    mutable_runtime_feature_state.SetCanvasInterventionsEnabled(
        canvas_base_feature_enabled);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(InterventionsWebContentsHelper);

}  // namespace fingerprinting_protection_interventions
