// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_PAGE_UI_H_

#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

class Profile;

namespace base {
class RefCountedMemory;
}

// The WebUIController used for the app launcher page UI.
class AppLauncherPageUI : public content::WebUIController {
 public:
  explicit AppLauncherPageUI(content::WebUI* web_ui);

  AppLauncherPageUI(const AppLauncherPageUI&) = delete;
  AppLauncherPageUI& operator=(const AppLauncherPageUI&) = delete;

  ~AppLauncherPageUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

 private:
  void OnHideWebStoreIconChanged();

  Profile* GetProfile() const;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_PAGE_UI_H_
