// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_

#include <string>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between SyncConsentScreen and its
// WebUI representation.
class SyncConsentScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"sync-consent",
                                                       "SyncConsentScreen"};

  virtual ~SyncConsentScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(bool is_lacros_enabled) = 0;

  // The screen is initially shown in a loading state.
  // When SyncScreenBehavior becomes Shown, this method should be called to
  // advance the screen to the loaded state.
  virtual void ShowLoadedStep(bool os_sync_lacros) = 0;

  // Set the minor mode flag, which controls whether we could use nudge
  // techinuque on the UI.
  virtual void SetIsMinorMode(bool value) = 0;

  virtual void RetrieveConsentIDs(::login::StringList& consent_description,
                                  const std::string& consent_confirmation,
                                  std::vector<int>& consent_description_ids,
                                  int& consent_confirmation_id) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<SyncConsentScreenView> AsWeakPtr() = 0;
};

// The sole implementation of the SyncConsentScreenView, using WebUI.
class SyncConsentScreenHandler final : public BaseScreenHandler,
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
  void Show(bool is_arc_restricted) override;
  void ShowLoadedStep(bool os_sync_lacros) override;
  void SetIsMinorMode(bool value) override;

  void RetrieveConsentIDs(::login::StringList& consent_description,
                          const std::string& consent_confirmation,
                          std::vector<int>& consent_description_ids,
                          int& consent_confirmation_id) override;
  base::WeakPtr<SyncConsentScreenView> AsWeakPtr() override;

 private:
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

  base::WeakPtrFactory<SyncConsentScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SYNC_CONSENT_SCREEN_HANDLER_H_
