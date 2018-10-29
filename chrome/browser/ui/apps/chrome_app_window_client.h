// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_APP_WINDOW_CLIENT_H_
#define CHROME_BROWSER_UI_APPS_CHROME_APP_WINDOW_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "extensions/browser/app_window/app_window_client.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

// The implementation of AppWindowClient for Chrome.
class ChromeAppWindowClient : public extensions::AppWindowClient {
 public:
  ChromeAppWindowClient();
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
  extensions::NativeAppWindow* CreateNativeAppWindow(
      extensions::AppWindow* window,
      extensions::AppWindow::CreateParams* params) override;
  void OpenDevToolsWindow(content::WebContents* web_contents,
                          const base::Closure& callback) override;
  bool IsCurrentChannelOlderThanDev() override;

  // Implemented in platform specific code.
  static extensions::NativeAppWindow* CreateNativeAppWindowImpl(
      extensions::AppWindow* window,
      const extensions::AppWindow::CreateParams& params);

  DISALLOW_COPY_AND_ASSIGN(ChromeAppWindowClient);
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_APP_WINDOW_CLIENT_H_
