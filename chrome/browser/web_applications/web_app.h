// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_

#include <iosfwd>
#include <string>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class WebApp {
 public:
  explicit WebApp(const AppId& app_id);
  ~WebApp();

  const AppId& app_id() const { return app_id_; }

  const std::string& name() const { return name_; }
  const std::string& description() const { return description_; }
  const std::string& launch_url() const { return launch_url_; }

  void SetName(const std::string& name);
  void SetDescription(const std::string& description);
  void SetLaunchUrl(const std::string& launch_url);

 private:
  const AppId app_id_;

  std::string name_;
  std::string description_;
  std::string launch_url_;

  DISALLOW_COPY_AND_ASSIGN(WebApp);
};

// For logging and debug purposes.
std::ostream& operator<<(std::ostream& out, const WebApp& app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
