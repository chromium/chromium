// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_UI_H_

#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

class ExtensionsUI : public content::WebContentsObserver,
                     public content::WebUIController {
 public:
  explicit ExtensionsUI(content::WebUI* web_ui);
  ExtensionsUI(const ExtensionsUI&) = delete;
  ExtensionsUI& operator=(const ExtensionsUI&) = delete;
  ~ExtensionsUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // content::WebContentsObserver:
  void DidStopLoading() override;

  // Called when developer mode is toggled.
  void OnDevModeChanged();

  // Tracks whether developer mode is enabled.
  BooleanPrefMember in_dev_mode_;

  WebuiLoadTimer webui_load_timer_;

  // Time the chrome://extensions page has been open.
  base::Optional<base::ElapsedTimer> timer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_UI_H_
