// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_UNINSTALL_APPLICATION_H_
#define CHROME_BROWSER_WIN_CONFLICTS_UNINSTALL_APPLICATION_H_

#include "base/strings/string16.h"

namespace uninstall_application {

// Uses UI automation to asynchronously open the Apps & Features page with the
// application name written in the search box, to filter out other applications.
void LaunchUninstallFlow(const base::string16& application_name);

}  // namespace uninstall_application

#endif  // CHROME_BROWSER_WIN_CONFLICTS_UNINSTALL_APPLICATION_H_
