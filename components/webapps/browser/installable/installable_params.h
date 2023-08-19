// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PARAMS_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PARAMS_H_

namespace webapps {

// This struct specifies the work to be done by the InstallableManager.
// Data is cached and fetched in the order specified in this struct.
// Processing halts immediately upon the first error unless |is_debug_mode| is
// true, otherwise, all tasks will be run and a complete list of errors will be
// returned.
struct InstallableParams {
 public:
  InstallableParams();
  ~InstallableParams();
  InstallableParams(const InstallableParams& other);

  // Check whether the current WebContents is eligible to be installed, i.e it:
  //  - is served over HTTPS
  //  - is not in an incognito profile.
  bool check_eligibility = false;

  // Check whether there is a fetchable, non-empty icon in the manifest
  // conforming to the primary icon size parameters.
  bool valid_primary_icon = false;

  // Whether to fetch web page metadata for installable checks if manifest is
  // not available.
  bool fetch_metadata = false;

  // Whether to prefer an icon with purpose 'maskable' for the primary icon.
  // An icon with purpose 'any' is still required for a valid manifest.
  bool prefer_maskable_icon = false;

  // Whether to fetch favicon for the primary icon if no manifest icon is
  // available.
  bool fetch_favicon = false;

  // Check whether the site has a manifest valid for a web app.
  bool valid_manifest = false;

  // If the manifest is being checked, check the display setting in the manifest
  // is a valid webapp display setting.
  bool check_webapp_manifest_display = true;

  // Whether to fetch the screenshots listed in the manifest.
  bool fetch_screenshots = false;

  // Check whether the site has a service worker controlling the manifest start
  // URL and the current URL. A value of true also indicates that the current
  // WebContents is a top-level frame.
  bool has_worker = false;

  // Whether or not to wait indefinitely for a service worker. If this is set to
  // false, the worker status will not be cached and will be re-checked if
  // GetData() is called again for the current page. Setting this to true means
  // that the callback will not be called for any site that does not install a
  // service worker.
  bool wait_for_worker = false;

  // True if the check should not short-circuit exit on errors, but continue
  // and accumulate all possible errors.
  bool is_debug_mode = false;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PARAMS_H_
