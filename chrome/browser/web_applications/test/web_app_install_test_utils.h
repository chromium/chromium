// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_TEST_UTILS_H_

#include <memory>
#include <string>

#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"

class GURL;
class Profile;

namespace web_app {
namespace test {

// Start the WebAppProvider and subsystems, and wait for startup to complete.
// Disables auto-install of system web apps and default web apps. Intended for
// unit tests, not browser tests.
void AwaitStartWebAppProviderAndSubsystems(Profile* profile);

AppId InstallDummyWebApp(Profile* profile,
                         const std::string& app_name,
                         const GURL& app_url);

// Synchronous version of InstallManager::InstallWebAppFromInfo.
// TODO (glenrob): Remove the duplicate of this in web_app_browsertest_util.h.
AppId InstallWebApp(Profile* profile, std::unique_ptr<WebApplicationInfo>);

}  // namespace test
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_TEST_UTILS_H_
