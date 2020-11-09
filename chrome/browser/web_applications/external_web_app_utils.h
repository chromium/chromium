// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_UTILS_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/external_install_options.h"

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace web_app {

class FileUtilsWrapper;

base::Optional<ExternalInstallOptions> ParseConfig(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& file,
    const base::Value& app_config);

base::Optional<WebApplicationInfoFactory> ParseOfflineManifest(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& file,
    const base::Value& offline_manifest);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_UTILS_H_
