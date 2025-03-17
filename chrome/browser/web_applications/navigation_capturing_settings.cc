// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_settings.h"

#include "chrome/browser/web_applications/link_capturing_features.h"
#include "content/public/common/content_features.h"
#include "ui/base/ui_base_features.h"

namespace web_app {

namespace {

// Causes all new auxiliary browser contexts to share the same window container
// type as where they were created from. For example, if an aux context was
// created from standalone PWA, then the new context will be created in a new
// window of the same PWA.
// If this is off, then all auxiliary contexts will be created as browser tabs.
const base::FeatureParam<bool> kEnableAuxContextKeepSameContainer{
    &features::kPwaNavigationCapturing, "aux_context_keep_same_container",
    /*default_value=*/false};

}  // namespace

// Keeping auxiliary contexts in an 'app' container was causing problems on
// initial Canary testing, see https://crbug.com/379181271 for more information.
// Either this will be rolled out separately or removed.
bool NavigationCapturingSettings::ShouldAuxiliaryContextsKeepSameContainer(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const GURL& url) {
  return apps::features::IsNavigationCapturingReimplEnabled() &&
         kEnableAuxContextKeepSameContainer.Get();
}

}  // namespace web_app
