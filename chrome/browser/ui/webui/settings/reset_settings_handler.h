// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_RESET_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_RESET_SETTINGS_HANDLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/profile_resetter/profile_reset_report.pb.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace content {
class WebUIDataSource;
}

class BrandcodeConfigFetcher;
class Profile;
class ProfileResetter;
class ResettableSettingsSnapshot;

namespace settings {

// Handler for
//  1) 'Reset Profile Settings' dialog
//  2) 'Powerwash' dialog (ChromeOS only)
class ResetSettingsHandler : public SettingsPageUIHandler {
 public:
  // Hash used by the Chrome Cleanup Tool when launching chrome with the reset
  // profile settings URL.
  static const char kCctResetSettingsHash[];

  ~ResetSettingsHandler() override;

  static ResetSettingsHandler* Create(
      content::WebUIDataSource* html_source, Profile* profile);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override;

 protected:
  explicit ResetSettingsHandler(Profile* profile);

  // Overriden in tests to substitute with a test version of ProfileResetter.
  virtual ProfileResetter* GetResetter();

  // Javascript callback to start clearing data.
  void HandleResetProfileSettings(const base::ListValue* args);

 private:
  // Retrieves the settings that will be reported, called from Javascript.
  void HandleGetReportedSettings(const base::ListValue* args);

  // Called once the settings that will be reported have been retrieved.
  void OnGetReportedSettingsDone(std::string callback_id);

  // Called when the reset profile dialog is shown.
  void OnShowResetProfileDialog(const base::ListValue* args);

  // Called when the reset profile dialog is hidden.
  void OnHideResetProfileDialog(const base::ListValue* args);

  // Called when the reset profile banner is shown.
  void OnHideResetProfileBanner(const base::ListValue* args);

  // Retrieve the triggered reset tool name, called from Javascript.
  void HandleGetTriggeredResetToolName(const base::ListValue* args);

  // Called when BrandcodeConfigFetcher completed fetching settings.
  void OnSettingsFetched();

  // Resets profile settings to default values. |send_settings| is true if user
  // gave their consent to upload broken settings to Google for analysis.
  void ResetProfile(
      const std::string& callback_id,
      bool send_settings,
      reset_report::ChromeResetReport::ResetRequestOrigin request_origin);

  // Closes the dialog once all requested settings has been reset.
  void OnResetProfileSettingsDone(
      std::string callback_id,
      bool send_feedback,
      reset_report::ChromeResetReport::ResetRequestOrigin request_origin);

#if defined(OS_CHROMEOS)
  // Will be called when powerwash dialog is shown.
  void OnShowPowerwashDialog(const base::ListValue* args);
#endif  // defined(OS_CHROMEOS)

  Profile* const profile_;

  std::unique_ptr<ProfileResetter> resetter_;

  std::unique_ptr<BrandcodeConfigFetcher> config_fetcher_;

  // Snapshot of settings before profile was reseted.
  std::unique_ptr<ResettableSettingsSnapshot> setting_snapshot_;

  // Contains Chrome brand code; empty for organic Chrome.
  std::string brandcode_;

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<ResetSettingsHandler> callback_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ResetSettingsHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_RESET_SETTINGS_HANDLER_H_
