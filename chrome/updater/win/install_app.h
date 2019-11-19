// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALL_APP_H_
#define CHROME_UPDATER_WIN_INSTALL_APP_H_

#include <string>

namespace updater {

// Sets the updater up, shows up a splash screen, then installs an application
// while displaying the UI progress window.
int InstallApp(const std::string& app_id);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALL_APP_H_
