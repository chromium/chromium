// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_

#include <memory>

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

// TODO(https://crbug.com/1427768): Delete this file.
class WebAppInstallTask {
 public:
  static std::unique_ptr<content::WebContents> CreateWebContents(
      Profile* profile);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
