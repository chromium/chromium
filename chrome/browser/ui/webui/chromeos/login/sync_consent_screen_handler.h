// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_

#include <string>
#include <unordered_map>

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class SyncConsentScreen;
}

namespace chromeos {

// Interface for dependency injection between SyncConsentScreen and its
// WebUI representation.
class SyncConsentScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"sync-consent"};

  virtual ~SyncConsentScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(ash::SyncConsentScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show(bool is_arc_restricted) = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Controls if the loading throbber is visible. This is used when
  // SyncScreenBehavior is unknown.
  virtual void SetThrobberVisible(bool visible) = 0;

  // Set the minor mode flag, which controls whether we could use nudge
  // techinuque on the UI.
  virtual void SetIsMinorMode(bool value) = 0;
};

// The sole implementation of the SyncConsentScreenView, using WebUI.
class SyncConsentScreenHandler : public BaseScreenHandler,
                                 public SyncConsentScreenView {
 public:
  using TView = SyncConsentScreenView;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Public for testing.
  enum class UserChoice { kDeclined = 0, kAccepted = 1, kMaxValue = kAccepted };

  explicit SyncConsentScreenHandler(JSCallsContainer* js_calls_container);

  SyncConsentScreenHandler(const SyncConsentScreenHandler&) = delete;
  SyncConsentScreenHandler& operator=(const SyncConsentScreenHandler&) = delete;

  ~SyncConsentScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // SyncConsentScreenView:
  void Bind(ash::SyncConsentScreen* screen) override;
  void Show(bool is_arc_restricted) override;
  void Hide() override;
  void SetThrobberVisible(bool visible) override;
  void SetIsMinorMode(bool value) override;

 private:
  // BaseScreenHandler:
  void Initialize() override;
  void RegisterMessages() override;

  // WebUI message handlers
  void HandleNonSplitSettingsContinue(
      const bool opted_in,
      const bool review_sync,
      const ::login::StringList& consent_description,
      const std::string& consent_confirmation);

  // WebUI message handlers for SplitSettingsSync.
  // TODO(https://crbug.com/1278325): Remove these.
  void HandleAcceptAndContinue(const ::login::StringList& consent_description,
                               const std::string& consent_confirmation);
  void HandleDeclineAndContinue(const ::login::StringList& consent_description,
                                const std::string& consent_confirmation);

  // Adds resource `resource_id` both to `builder` and to `known_string_ids_`.
  void RememberLocalizedValue(const std::string& name,
                              const int resource_id,
                              ::login::LocalizedValuesBuilder* builder);
  void RememberLocalizedValueWithDeviceName(
      const std::string& name,
      const int resource_id,
      ::login::LocalizedValuesBuilder* builder);

  // Resource IDs of the displayed strings.
  std::unordered_map<std::string, int> known_strings_;

  ash::SyncConsentScreen* screen_ = nullptr;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::SyncConsentScreenHandler;
using ::chromeos::SyncConsentScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_
