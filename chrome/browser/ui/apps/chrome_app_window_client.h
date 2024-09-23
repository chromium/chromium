// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_APP_WINDOW_CLIENT_H_
#define CHROME_BROWSER_UI_APPS_CHROME_APP_WINDOW_CLIENT_H_

#include "extensions/browser/app_window/app_window_client.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

// The implementation of AppWindowClient for Chrome.
class ChromeAppWindowClient : public extensions::AppWindowClient {
 public:
  ChromeAppWindowClient();

  ChromeAppWindowClient(const ChromeAppWindowClient&) = delete;
  ChromeAppWindowClient& operator=(const ChromeAppWindowClient&) = delete;

  ~ChromeAppWindowClient() override;

  // Get the LazyInstance for ChromeAppWindowClient.
  static ChromeAppWindowClient* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ChromeAppWindowClient>;

  // extensions::AppWindowClient
  extensions::AppWindow* CreateAppWindow(
      content::BrowserContext* context,
      const extensions::Extension* extension) override;
  extensions::AppWindow* CreateAppWindowForLockScreenAction(
      content::BrowserContext* context,
      const extensions::Extension* extension,
      extensions::api::app_runtime::ActionType action) override;
  std::unique_ptr<extensions::NativeAppWindow> CreateNativeAppWindow(
      extensions::AppWindow* window,
      extensions::AppWindow::CreateParams* params) override;
  void OpenDevToolsWindow(content::WebContents* web_contents,
                          base::OnceClosure callback) override;
  bool IsCurrentChannelOlderThanDev() override;

  // Implemented in platform specific code.
  static extensions::NativeAppWindow* CreateNativeAppWindowImpl(
      extensions::AppWindow* window,
      const extensions::AppWindow::CreateParams& params);
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_APP_WINDOW_CLIENT_H_
