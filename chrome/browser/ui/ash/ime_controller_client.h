// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_IME_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_IME_CONTROLLER_CLIENT_H_

#include "ash/public/mojom/ime_controller.mojom.h"
#include "ash/public/mojom/ime_info.mojom-forward.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"

// Connects the ImeController in ash to the InputMethodManagerImpl in chrome.
class ImeControllerClient
    : public ash::mojom::ImeControllerClient,
      public chromeos::input_method::InputMethodManager::Observer,
      public chromeos::input_method::InputMethodManager::ImeMenuObserver,
      public chromeos::input_method::ImeKeyboard::Observer,
      public ui::ime::InputMethodMenuManager::Observer {
 public:
  explicit ImeControllerClient(
      chromeos::input_method::InputMethodManager* manager);
  ~ImeControllerClient() override;

  // Initializes and connects to ash.
  void Init();

  // Tests can shim in a mock mojo interface for the ash controller.
  void InitForTesting(
      mojo::PendingRemote<ash::mojom::ImeController> controller);

  static ImeControllerClient* Get();

  // Sets whether the list of IMEs is managed by device policy.
  void SetImesManagedByPolicy(bool managed);

  // ash::mojom::ImeControllerClient:
  void SwitchToNextIme() override;
  void SwitchToLastUsedIme() override;
  void SwitchImeById(const std::string& id, bool show_message) override;
  void ActivateImeMenuItem(const std::string& key) override;
  void SetCapsLockEnabled(bool caps_enabled) override;
  void UpdateMirroringState(bool mirroring_enabled) override;
  void UpdateCastingState(bool casting_enabled) override;
  void OverrideKeyboardKeyset(chromeos::input_method::mojom::ImeKeyset keyset,
                              OverrideKeyboardKeysetCallback callback) override;
  void ShowModeIndicator() override;

  // chromeos::input_method::InputMethodManager::Observer:
  void InputMethodChanged(chromeos::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // chromeos::input_method::InputMethodManager::ImeMenuObserver:
  void ImeMenuActivationChanged(bool is_active) override;
  void ImeMenuListChanged() override;
  void ImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<chromeos::input_method::InputMethodManager::MenuItem>&
          items) override;

  // ui::ime::InputMethodMenuManager::Observer:
  void InputMethodMenuItemChanged(
      ui::ime::InputMethodMenuManager* manager) override;

  // chromeos::input_method::ImeKeyboard::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnLayoutChanging(const std::string& layout_name) override;

  // chromeos::input_method::InputMethodManager::Observer
  void OnExtraInputEnabledStateChange(bool is_extra_input_options_enabled,
                                      bool is_emoji_enabled,
                                      bool is_handwriting_enabled,
                                      bool is_voice_enabled) override;

  void FlushMojoForTesting();

 private:
  // Binds this object to its mojo interface and sets it as the ash client.
  void BindAndSetClient();

  // Converts IME information from |descriptor| into the ash mojo format.
  ash::mojom::ImeInfoPtr GetAshImeInfo(
      const chromeos::input_method::InputMethodDescriptor& descriptor) const;

  // Sends information about current and available IMEs to ash.
  void RefreshIme();

  chromeos::input_method::InputMethodManager* const input_method_manager_;

  // Binds this object to the mojo interface.
  mojo::Receiver<ash::mojom::ImeControllerClient> receiver_{this};

  // ImeController remote in ash.
  mojo::Remote<ash::mojom::ImeController> ime_controller_;

  DISALLOW_COPY_AND_ASSIGN(ImeControllerClient);
};

#endif  // CHROME_BROWSER_UI_ASH_IME_CONTROLLER_CLIENT_H_
