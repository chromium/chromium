// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_VIDS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_VIDS_H_

#include "chrome/browser/web_applications/external_install_options.h"

namespace web_app {

// Returns the config for preinstalling the Vids app.
ExternalInstallOptions GetConfigForVids(bool is_standalone_tabbed);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_VIDS_H_
