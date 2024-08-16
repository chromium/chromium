// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PARAMS_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PARAMS_H_

namespace webapps {

enum class InstallableCriteria {
  // Fetch data only, do not check if the page satisfied install criteria.
  kDoNotCheck,
  // The page is installable if it has a valid manifest that contains valid
  // icons.
  kValidManifestWithIcons,
  // Check the manifest valid ignoring the display setting.
  kValidManifestIgnoreDisplay,
  // The page can be installable if required info (name, icon, display mode...)
  // is provided implicitly with meta tags, favicon, etc.
  kImplicitManifestFieldsHTML,
  // Page at the root level of an origin can be installable without manifest
  // if required info is provided implicitly.
  kNoManifestAtRootScope
};

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

  // Check whether the site manifest and web page metadata provided sufficient
  // info for installing the web app.
  InstallableCriteria installable_criteria = InstallableCriteria::kDoNotCheck;

  // Whether to fetch the screenshots listed in the manifest.
  bool fetch_screenshots = false;

  // True if the check should not short-circuit exit on errors, but continue
  // and accumulate all possible errors.
  bool is_debug_mode = false;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PARAMS_H_
