// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_CHROMECAST_AUTOMATION_INTERNAL_API_DELEGATE_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_CHROMECAST_AUTOMATION_INTERNAL_API_DELEGATE_H_

#include "extensions/browser/api/automation_internal/automation_internal_api_delegate.h"

namespace extensions {

// A delegate for chromecast specific automation api logic.
class ChromecastAutomationInternalApiDelegate
    : public AutomationInternalApiDelegate {
 public:
  ChromecastAutomationInternalApiDelegate();

  ChromecastAutomationInternalApiDelegate(
      const ChromecastAutomationInternalApiDelegate&) = delete;
  ChromecastAutomationInternalApiDelegate& operator=(
      const ChromecastAutomationInternalApiDelegate&) = delete;

  ~ChromecastAutomationInternalApiDelegate() override;

  bool CanRequestAutomation(const Extension* extension,
                            const AutomationInfo* automation_info,
                            content::WebContents* contents) override;
  bool GetTabById(int tab_id,
                  content::BrowserContext* browser_context,
                  bool include_incognito,
                  content::WebContents** contents,
                  std::string* error_msg) override;
  int GetTabId(content::WebContents* contents) override;
  content::WebContents* GetActiveWebContents(
      ExtensionFunction* function) override;
  bool EnableTree(const ui::AXTreeID& tree_id) override;
  void EnableDesktop() override;
  ui::AXTreeID GetAXTreeID() override;
  void SetAutomationEventRouterInterface(
      AutomationEventRouterInterface* router) override;
  content::BrowserContext* GetActiveUserContext() override;
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_CHROMECAST_AUTOMATION_INTERNAL_API_DELEGATE_H_
