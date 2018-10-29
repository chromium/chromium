// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_

class Profile;

namespace web_app {

bool AllowWebAppInstallation(Profile* profile);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UTILS_H_
