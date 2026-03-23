// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_CROS_ISOLATED_WEB_APP_ENABLER_H_
#define CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_CROS_ISOLATED_WEB_APP_ENABLER_H_

#include "base/component_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace ash {

// This class enables Blink extensions for IWAs on ChromeOS when the
// `kCrosIsolatedWebAppSetShape` feature flag is enabled.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ISOLATED_WEB_APP)
    CrosIsolatedWebAppEnabler
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CrosIsolatedWebAppEnabler> {
 public:
  CrosIsolatedWebAppEnabler(const CrosIsolatedWebAppEnabler&) = delete;
  CrosIsolatedWebAppEnabler& operator=(const CrosIsolatedWebAppEnabler&) =
      delete;

  ~CrosIsolatedWebAppEnabler() override;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<CrosIsolatedWebAppEnabler>;

  explicit CrosIsolatedWebAppEnabler(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_CROS_ISOLATED_WEB_APP_ENABLER_H_
