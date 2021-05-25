// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_WEB_APP_URL_HANDLING_STARTUP_UTILS_H_
#define CHROME_BROWSER_UI_STARTUP_WEB_APP_URL_HANDLING_STARTUP_UTILS_H_

#include <vector>

#include "base/callback.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

class Browser;
class GURL;

namespace web_app {
namespace startup {

using FinalizeWebAppLaunchCallback =
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)>;

// If |command_line| contains a single URL argument and that URL matches URL
// handling registration from installed web apps, show app options to user and
// launch one if accepted.
// Returns true if launching an app, false otherwise.
bool MaybeLaunchUrlHandlerWebAppFromCmd(const base::CommandLine& command_line,
                                        const base::FilePath& cur_dir,
                                        FinalizeWebAppLaunchCallback callback);

// Same as MaybeLaunchUrlHandlerWebAppFromCmd but check if |urls| contains a
// single URL.
bool MaybeLaunchUrlHandlerWebAppFromUrls(const std::vector<GURL>& urls,
                                         FinalizeWebAppLaunchCallback callback);

}  // namespace startup
}  // namespace web_app

#endif  // CHROME_BROWSER_UI_STARTUP_WEB_APP_URL_HANDLING_STARTUP_UTILS_H_
