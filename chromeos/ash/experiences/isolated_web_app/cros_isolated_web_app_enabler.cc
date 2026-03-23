// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/cros_isolated_web_app_enabler.h"

#include "chromeos/constants/chromeos_features.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"

namespace ash {

CrosIsolatedWebAppEnabler::CrosIsolatedWebAppEnabler(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CrosIsolatedWebAppEnabler>(*web_contents) {}

CrosIsolatedWebAppEnabler::~CrosIsolatedWebAppEnabler() = default;

void CrosIsolatedWebAppEnabler::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!chromeos::features::IsCrosIsolatedWebAppSetShapeEnabled()) {
    return;
  }

  if (!navigation_handle->GetURL().SchemeIs(webapps::kIsolatedAppScheme)) {
    return;
  }

  blink::RuntimeFeatureStateContext& context =
      navigation_handle->GetMutableRuntimeFeatureStateContext();
  context.SetBlinkExtensionChromeOSEnabled(true);
  context.SetBlinkExtensionChromeOSIsolatedWebAppSetShapeEnabled(true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CrosIsolatedWebAppEnabler);

}  // namespace ash
