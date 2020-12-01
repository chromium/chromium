// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_UTILS_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace web_app {

class FileUtilsWrapper;

using OptionsOrError = absl::variant<ExternalInstallOptions, std::string>;

OptionsOrError ParseConfig(FileUtilsWrapper& file_utils,
                           const base::FilePath& dir,
                           const base::FilePath& file,
                           const base::Value& app_config);

using WebApplicationInfoFactoryOrError =
    absl::variant<WebApplicationInfoFactory, std::string>;

WebApplicationInfoFactoryOrError ParseOfflineManifest(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& file,
    const base::Value& offline_manifest);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_UTILS_H_
