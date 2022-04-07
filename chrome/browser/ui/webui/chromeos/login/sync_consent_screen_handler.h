// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_

#include <string>
#include <unordered_map>

#include "base/values.h"
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

  // The screen is initially shown in a loading state.
  // When SyncScreenBehavior becomes Shown, this method should be called to
  // advance the screen to the loaded state.
  virtual void ShowLoadedStep() = 0;

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

  SyncConsentScreenHandler();

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
  void ShowLoadedStep() override;
  void SetIsMinorMode(bool value) override;

 private:
  // BaseScreenHandler:
  void InitializeDeprecated() override;
  void RegisterMessages() override;

  // WebUI message handlers
  void HandleContinue(const bool opted_in,
                      const bool review_sync,
                      const base::Value::List& consent_description_list,
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
