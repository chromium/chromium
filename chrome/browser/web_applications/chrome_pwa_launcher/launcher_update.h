// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_UPDATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_UPDATE_H_

#include <vector>

namespace base {
class FilePath;
}

namespace web_app {

// For each launcher in |launcher_paths|, cleans up old versions of the
// launcher, then replaces it with a hardlink or copy of the latest launcher
// version. Appends "_old" to the old launcher's filename, marking it for
// deletion next time the launcher runs. If update fails, rolls back changes.
void UpdatePwaLaunchers(std::vector<base::FilePath> launcher_paths);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_UPDATE_H_
