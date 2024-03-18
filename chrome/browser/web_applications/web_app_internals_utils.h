// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INTERNALS_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INTERNALS_UTILS_H_

#include <string_view>

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace web_app {

using ReadErrorLogCallback =
    base::OnceCallback<void(Result, base::Value error_log)>;

void ReadErrorLog(const base::FilePath& web_apps_directory,
                  std::string_view subsystem_name,
                  ReadErrorLogCallback callback);

using FileIoCallback = base::OnceCallback<void(Result)>;

void WriteErrorLog(const base::FilePath& web_apps_directory,
                   std::string_view subsystem_name,
                   base::Value error_log,
                   FileIoCallback callback);

void ClearErrorLog(const base::FilePath& web_apps_directory,
                   std::string_view subsystem_name,
                   FileIoCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INTERNALS_UTILS_H_
