// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

// Settings page UI handler that checks four areas of browser safety: browser
// updates, password leaks, malicious extensions, and unwanted software.
class SafetyCheckHandler : public settings::SettingsPageUIHandler {
 public:
  SafetyCheckHandler();
  ~SafetyCheckHandler() override;

  // Triggers all four of the browser safety checks.
  // Note: since the checks deal with sensitive user information, this method
  // should only be called as a result of an explicit user action.
  void PerformSafetyCheck();

  // Each triggers a corresponding check and calls the provided callback on
  // completion.
  void CheckUpdates(VersionUpdater* updater,
                    const VersionUpdater::StatusCallback& update_callback);

 private:
  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {}

  // Callbacks that get triggered when each check completes.
  void OnUpdateCheckResult(VersionUpdater::Status status,
                           int progress,
                           bool rollback,
                           const std::string& version,
                           int64_t update_size,
                           const base::string16& message);

  std::unique_ptr<VersionUpdater> version_updater_;

  DISALLOW_COPY_AND_ASSIGN(SafetyCheckHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_
