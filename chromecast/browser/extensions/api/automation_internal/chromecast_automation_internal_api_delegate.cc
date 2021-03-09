// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/automation_internal/chromecast_automation_internal_api_delegate.h"

#include <memory>

#include "base/notreached.h"
#include "chromecast/browser/extensions/api/tabs/tabs_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/api/automation.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/automation.h"
#include "extensions/common/permissions/permissions_data.h"

#if defined(USE_AURA)
#include "chromecast/browser/ui/aura/accessibility/automation_manager_aura.h"
#endif

namespace extensions {

ChromecastAutomationInternalApiDelegate::
    ChromecastAutomationInternalApiDelegate() {}

ChromecastAutomationInternalApiDelegate::
    ~ChromecastAutomationInternalApiDelegate() {}

bool ChromecastAutomationInternalApiDelegate::CanRequestAutomation(
    const Extension* extension,
    const AutomationInfo* automation_info,
    content::WebContents* contents) {
  if (automation_info->desktop)
    return true;

  const GURL& url = contents->GetURL();
  if (automation_info->matches.MatchesURL(url))
    return true;

  return false;
}

bool ChromecastAutomationInternalApiDelegate::GetTabById(
    int tab_id,
    content::BrowserContext* browser_context,
    bool include_incognito,
    content::WebContents** contents,
    std::string* error_msg) {
  NOTIMPLEMENTED();
  return false;
}

int ChromecastAutomationInternalApiDelegate::GetTabId(
    content::WebContents* contents) {
  NOTIMPLEMENTED();
  return 0;
}

content::WebContents*
ChromecastAutomationInternalApiDelegate::GetActiveWebContents(
    ExtensionFunction* function) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool ChromecastAutomationInternalApiDelegate::EnableTree(
    const ui::AXTreeID& tree_id) {
  return false;
}

void ChromecastAutomationInternalApiDelegate::EnableDesktop() {
  AutomationManagerAura::GetInstance()->Enable();
}

ui::AXTreeID ChromecastAutomationInternalApiDelegate::GetAXTreeID() {
  return AutomationManagerAura::GetInstance()->ax_tree_id();
}

void ChromecastAutomationInternalApiDelegate::SetEventBundleSink(
    ui::AXEventBundleSink* sink) {
  AutomationManagerAura::GetInstance()->set_event_bundle_sink(sink);
}

content::BrowserContext*
ChromecastAutomationInternalApiDelegate::GetActiveUserContext() {
  // Active user profile is set by Chromecast
  return nullptr;
}

}  // namespace extensions
