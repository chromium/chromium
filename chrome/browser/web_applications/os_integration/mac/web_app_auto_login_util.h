// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_WEB_APP_AUTO_LOGIN_UTIL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_WEB_APP_AUTO_LOGIN_UTIL_H_

namespace base {
class FilePath;
}

namespace web_app {

// This interface wraps base::mac::AddToLoginItems and RemoveFromLoginItems
// to allow tests to override and mock out their behavior by replacing the
// default WebAppAutoLoginUtil instance with their own.
class WebAppAutoLoginUtil {
 public:
  WebAppAutoLoginUtil() = default;
  WebAppAutoLoginUtil(const WebAppAutoLoginUtil&) = delete;
  WebAppAutoLoginUtil& operator=(const WebAppAutoLoginUtil&) = delete;

  static WebAppAutoLoginUtil* GetInstance();

  static void SetInstanceForTesting(WebAppAutoLoginUtil* auto_login_util);

  // Adds the specified app to the list of login items.
  virtual void AddToLoginItems(const base::FilePath& app_bundle_path,
                               bool hide_on_startup);

  // Removes the specified app from the list of login items.
  virtual void RemoveFromLoginItems(const base::FilePath& app_bundle_path);

 protected:
  virtual ~WebAppAutoLoginUtil() = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_WEB_APP_AUTO_LOGIN_UTIL_H_
