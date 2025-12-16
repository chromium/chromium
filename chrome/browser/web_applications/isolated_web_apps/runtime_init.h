// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_RUNTIME_INIT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_RUNTIME_INIT_H_

#include "base/types/pass_key.h"

class BrowserProcessImpl;
class TestingBrowserProcess;

namespace web_app {

// Is normally called once at browser startup by either BrowserProcessImpl or
// TestingBrowserProcess.
void InitializeIsolatedWebAppRuntime(
    base::PassKey<BrowserProcessImpl, TestingBrowserProcess>);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_RUNTIME_INIT_H_
