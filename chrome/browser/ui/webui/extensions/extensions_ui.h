// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_UI_H_

#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

class ExtensionsUIConfig : public content::WebUIConfig {
 public:
  ExtensionsUIConfig();
  ~ExtensionsUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

class ExtensionsUI : public content::WebUIController {
 public:
  explicit ExtensionsUI(content::WebUI* web_ui);
  ExtensionsUI(const ExtensionsUI&) = delete;
  ExtensionsUI& operator=(const ExtensionsUI&) = delete;
  ~ExtensionsUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // Called when developer mode is toggled.
  void OnDevModeChanged();

  // Tracks whether developer mode is enabled.
  BooleanPrefMember in_dev_mode_;

  WebuiLoadTimer webui_load_timer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_UI_H_
