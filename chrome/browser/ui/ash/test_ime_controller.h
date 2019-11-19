// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_IME_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_TEST_IME_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/mojom/ime_controller.mojom.h"
#include "ash/public/mojom/ime_info.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class TestImeController : ash::mojom::ImeController {
 public:
  TestImeController();
  ~TestImeController() override;

  // Returns a mojo remote for this object.
  mojo::PendingRemote<ash::mojom::ImeController> CreateRemote();

  // ash::mojom::ImeController:
  void SetClient(
      mojo::PendingRemote<ash::mojom::ImeControllerClient> client) override;
  void RefreshIme(const std::string& current_ime_id,
                  std::vector<ash::mojom::ImeInfoPtr> available_imes,
                  std::vector<ash::mojom::ImeMenuItemPtr> menu_items) override;
  void SetImesManagedByPolicy(bool managed) override;
  void ShowImeMenuOnShelf(bool show) override;
  void UpdateCapsLockState(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override;
  void SetExtraInputOptionsEnabledState(bool is_extra_input_options_enabled,
                                        bool is_emoji_enabled,
                                        bool is_handwriting_enabled,
                                        bool is_voice_enabled) override;
  void ShowModeIndicator(const gfx::Rect& anchor_bounds,
                         const base::string16& text) override;

  // The most recent values received via mojo.
  std::string current_ime_id_;
  std::vector<ash::mojom::ImeInfoPtr> available_imes_;
  std::vector<ash::mojom::ImeMenuItemPtr> menu_items_;
  bool managed_by_policy_ = false;
  bool show_ime_menu_on_shelf_ = false;
  bool show_mode_indicator_ = false;
  bool is_caps_lock_enabled_ = false;
  std::string keyboard_layout_name_;
  bool is_extra_input_options_enabled_ = false;
  bool is_emoji_enabled_ = false;
  bool is_handwriting_enabled_ = false;
  bool is_voice_enabled_ = false;

 private:
  mojo::Receiver<ash::mojom::ImeController> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(TestImeController);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_IME_CONTROLLER_H_
