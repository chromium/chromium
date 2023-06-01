// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_H_

#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

// The Web UI controller for the chrome://policy page.
class PolicyUI : public content::WebUIController {
 public:
  explicit PolicyUI(content::WebUI* web_ui);

  PolicyUI(const PolicyUI&) = delete;
  PolicyUI& operator=(const PolicyUI&) = delete;

  ~PolicyUI() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_H_
