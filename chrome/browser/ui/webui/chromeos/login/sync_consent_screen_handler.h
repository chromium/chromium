// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_

#include <unordered_set>

#include "base/macros.h"
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
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Controls if the loading throbber is visible. This is used when
  // SyncScreenBehavior is unknown.
  virtual void SetThrobberVisible(bool visible) = 0;
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
  ~SyncConsentScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // SyncConsentScreenView:
  void Bind(ash::SyncConsentScreen* screen) override;
  void Show() override;
  void Hide() override;
  void SetThrobberVisible(bool visible) override;

 private:
  // BaseScreenHandler:
  void Initialize() override;
  void RegisterMessages() override;

  // WebUI message handlers
  void HandleContinueAndReview(const ::login::StringList& consent_description,
                               const std::string& consent_confirmation);
  void HandleContinueWithDefaults(
      const ::login::StringList& consent_description,
      const std::string& consent_confirmation);

  // WebUI message handlers for SplitSettingsSync.
  void HandleAcceptAndContinue(const ::login::StringList& consent_description,
                               const std::string& consent_confirmation);
  void HandleDeclineAndContinue(const ::login::StringList& consent_description,
                                const std::string& consent_confirmation);

  // Helper for the accept and decline cases.
  void Continue(const ::login::StringList& consent_description,
                const std::string& consent_confirmation,
                UserChoice choice);

  // Adds resource `resource_id` both to `builder` and to `known_string_ids_`.
  void RememberLocalizedValue(const std::string& name,
                              const int resource_id,
                              ::login::LocalizedValuesBuilder* builder);

  // Resource IDs of the displayed strings.
  std::unordered_set<int> known_string_ids_;

  ash::SyncConsentScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SyncConsentScreenHandler);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::SyncConsentScreenHandler;
using ::chromeos::SyncConsentScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_
