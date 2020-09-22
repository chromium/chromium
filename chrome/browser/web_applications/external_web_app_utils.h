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

struct ExternalConfigParseResult {
  enum Type {
    kEnabled,
    kDisabled,
    kError,
  };

  static ExternalConfigParseResult Enabled(ExternalInstallOptions options);
  static ExternalConfigParseResult Disabled();
  static ExternalConfigParseResult Error();

  ~ExternalConfigParseResult();
  ExternalConfigParseResult(const ExternalConfigParseResult&) = delete;
  ExternalConfigParseResult(ExternalConfigParseResult&&);
  ExternalConfigParseResult& operator=(const ExternalConfigParseResult&) =
      delete;

  const Type type;

  // Set iff kEnabled.
  const base::Optional<ExternalInstallOptions> options;

 private:
  ExternalConfigParseResult(Type type,
                            base::Optional<ExternalInstallOptions> options);
};

// TODO(https://crbug.com/1128801): Record and log parsing errors more
// effectively. At the moment they're indistinguishable from disabled apps to
// the caller.
ExternalConfigParseResult ParseConfig(FileUtilsWrapper& file_utils,
                                      const base::FilePath& dir,
                                      const base::FilePath& file,
                                      const std::string& user_type,
                                      const base::Value& app_config);

base::Optional<WebApplicationInfoFactory> ParseOfflineManifest(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& file,
    const base::Value& offline_manifest);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_UTILS_H_
