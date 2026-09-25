// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_CROS_ISOLATED_WEB_APP_ENABLER_H_
#define CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_CROS_ISOLATED_WEB_APP_ENABLER_H_

#include "base/component_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace ash {

// This class enables Blink extensions for IWAs on ChromeOS when the
// `blink::features::kSetShape` feature flag is enabled.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ISOLATED_WEB_APP)
    CrosIsolatedWebAppEnabler : public content::WebContentsObserver {
 public:
  explicit CrosIsolatedWebAppEnabler(content::WebContents* web_contents);
  CrosIsolatedWebAppEnabler(const CrosIsolatedWebAppEnabler&) = delete;
  CrosIsolatedWebAppEnabler& operator=(const CrosIsolatedWebAppEnabler&) =
      delete;

  ~CrosIsolatedWebAppEnabler() override;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_CROS_ISOLATED_WEB_APP_ENABLER_H_
