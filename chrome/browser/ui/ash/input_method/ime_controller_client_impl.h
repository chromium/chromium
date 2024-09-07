// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_IME_CONTROLLER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_IME_CONTROLLER_CLIENT_IMPL_H_

#include "ash/public/cpp/ime_controller.h"
#include "ash/public/cpp/ime_info.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_manager.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"

// Connects the ImeController in ash to the InputMethodManagerImpl in chrome.
class ImeControllerClientImpl
    : public ash::ImeControllerClient,
      public ash::input_method::InputMethodManager::Observer,
      public ash::input_method::InputMethodManager::ImeMenuObserver,
      public ash::input_method::ImeKeyboard::Observer,
      public ui::ime::InputMethodMenuManager::Observer {
 public:
  explicit ImeControllerClientImpl(
      ash::input_method::InputMethodManager* manager);
  ImeControllerClientImpl(const ImeControllerClientImpl&) = delete;
  ImeControllerClientImpl& operator=(const ImeControllerClientImpl&) = delete;
  ~ImeControllerClientImpl() override;

  // Initializes and connects to ash.
  void Init();

  // Tests can shim in a mock interface for the ash controller.
  void InitForTesting(ash::ImeController* controller);

  static ImeControllerClientImpl* Get();

  // Sets whether the list of IMEs is managed by device policy.
  void SetImesManagedByPolicy(bool managed);

  // ash::ImeControllerClient:
  void SwitchToNextIme() override;
  void SwitchToLastUsedIme() override;
  void SwitchImeById(const std::string& id, bool show_message) override;
  void ActivateImeMenuItem(const std::string& key) override;
  void SetCapsLockEnabled(bool caps_enabled) override;
  void OverrideKeyboardKeyset(ash::input_method::ImeKeyset keyset,
                              OverrideKeyboardKeysetCallback callback) override;
  void ShowModeIndicator() override;

  // ash::input_method::InputMethodManager::Observer:
  void InputMethodChanged(ash::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // ash::input_method::InputMethodManager::ImeMenuObserver:
  void ImeMenuActivationChanged(bool is_active) override;
  void ImeMenuListChanged() override;
  void ImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<ash::input_method::InputMethodManager::MenuItem>& items)
      override;

  // ui::ime::InputMethodMenuManager::Observer:
  void InputMethodMenuItemChanged(
      ui::ime::InputMethodMenuManager* manager) override;

  // ash::input_method::ImeKeyboard::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnLayoutChanging(const std::string& layout_name) override;

  // ash::input_method::InputMethodManager::Observer
  void OnExtraInputEnabledStateChange(bool is_extra_input_options_enabled,
                                      bool is_emoji_enabled,
                                      bool is_handwriting_enabled,
                                      bool is_voice_enabled) override;

 private:
  void InitAndSetClient();

  // Converts IME information from |descriptor| into the ash format.
  ash::ImeInfo GetAshImeInfo(
      const ash::input_method::InputMethodDescriptor& descriptor) const;

  // Sends information about current and available IMEs to ash.
  void RefreshIme();

  const raw_ptr<ash::input_method::InputMethodManager> input_method_manager_;

  // ImeController in ash.
  raw_ptr<ash::ImeController> ime_controller_ = nullptr;

  base::ScopedObservation<ash::input_method::ImeKeyboard,
                          ash::input_method::ImeKeyboard::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_IME_CONTROLLER_CLIENT_IMPL_H_
