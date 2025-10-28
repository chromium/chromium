// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/interventions/browser/canvas_interventions_web_contents_helper.h"

#include "base/feature_list.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
class StoragePartition;
class ServiceWorkerContext;
}  // namespace content

namespace fingerprinting_protection_interventions {

void CanvasInterventionsWebContentsHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  auto& mutable_runtime_feature_state =
      navigation_handle->GetMutableRuntimeFeatureStateContext();
  const bool canvas_base_feature_enabled =
      features::ShouldBlockCanvasReadbackForIncognitoState(is_incognito_);

  if (mutable_runtime_feature_state.IsBlockCanvasReadbackEnabled() !=
      canvas_base_feature_enabled) {
    mutable_runtime_feature_state.SetBlockCanvasReadbackEnabled(
        canvas_base_feature_enabled);
  }
}

// private
CanvasInterventionsWebContentsHelper::CanvasInterventionsWebContentsHelper(
    content::WebContents* web_contents,
    bool is_incognito)
    : content::WebContentsUserData<CanvasInterventionsWebContentsHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      is_incognito_(is_incognito) {
}

CanvasInterventionsWebContentsHelper::~CanvasInterventionsWebContentsHelper() =
    default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(CanvasInterventionsWebContentsHelper);

}  // namespace fingerprinting_protection_interventions
