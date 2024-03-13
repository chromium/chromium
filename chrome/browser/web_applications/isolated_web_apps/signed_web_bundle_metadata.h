// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_METADATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_METADATA_H_

#include <string>

#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_install_info.h"

class Profile;

namespace web_app {

class WebAppProvider;

// This class keeps metadata of a Signed Web Bundle. The data is used to
// populate installation UI, so that the user can review and confirm the bundle
// they are installing.
class SignedWebBundleMetadata {
 public:
  using SignedWebBundleMetadataCallback = base::OnceCallback<void(
      base::expected<SignedWebBundleMetadata, std::string>)>;

  // Performs installation checks for the bundle specified by |source|. If
  // bundle passes the checks, runs |callback| with the expected
  // SignedWebBundleMetaData. If bundle fails the checks, runs |callback| with
  // the unexpected error.
  static void Create(Profile* profile,
                     WebAppProvider* provider,
                     const IsolatedWebAppUrlInfo& url_info,
                     const IwaSourceBundleWithMode& source,
                     SignedWebBundleMetadataCallback callback);

  static SignedWebBundleMetadata CreateForTesting(
      const IsolatedWebAppUrlInfo& url_info,
      const IwaSourceBundleWithMode& source,
      const std::u16string& app_name,
      const base::Version& version,
      const IconBitmaps& icons);

  ~SignedWebBundleMetadata();
  SignedWebBundleMetadata(const SignedWebBundleMetadata&);
  SignedWebBundleMetadata& operator=(const SignedWebBundleMetadata&);

  const IsolatedWebAppUrlInfo& url_info() const { return url_info_; }

  const webapps::AppId& app_id() const { return url_info_.app_id(); }

  const std::u16string& app_name() const { return app_name_; }

  const base::Version& version() const { return version_; }

  const IconBitmaps& icons() const { return icons_; }

  bool operator==(const SignedWebBundleMetadata& other) const;

 private:
  SignedWebBundleMetadata(const IsolatedWebAppUrlInfo& url_info,
                          const IwaSourceBundleWithMode& source,
                          const std::u16string& app_name,
                          const base::Version& version,
                          const IconBitmaps& icons);

  IsolatedWebAppUrlInfo url_info_;
  std::u16string app_name_;
  base::Version version_;
  IconBitmaps icons_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_METADATA_H_
