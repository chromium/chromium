// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_NON_INSTALLED_BUNDLE_INSPECTION_CONTEXT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_NON_INSTALLED_BUNDLE_INSPECTION_CONTEXT_H_

#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Indicates that the specific instance of |WebContents| serves data for an IWA
// operation (install/update/bundle metadata reading). This allows components
// that operate on the same instance of |WebContents| to share this information.
class NonInstalledBundleInspectionContext
    : public content::WebContentsUserData<NonInstalledBundleInspectionContext> {
 public:
  NonInstalledBundleInspectionContext(
      const NonInstalledBundleInspectionContext&) = delete;
  NonInstalledBundleInspectionContext& operator=(
      const NonInstalledBundleInspectionContext&) = delete;
  NonInstalledBundleInspectionContext(NonInstalledBundleInspectionContext&&) =
      delete;
  NonInstalledBundleInspectionContext& operator=(
      NonInstalledBundleInspectionContext&&) = delete;

  ~NonInstalledBundleInspectionContext() override;

  const IwaSourceWithMode& source() const;

 private:
  NonInstalledBundleInspectionContext(content::WebContents* web_contents,
                                      IwaSourceWithMode source);
  friend class content::WebContentsUserData<
      NonInstalledBundleInspectionContext>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  IwaSourceWithMode source_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_NON_INSTALLED_BUNDLE_INSPECTION_CONTEXT_H_
