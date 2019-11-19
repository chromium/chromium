// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"

#include <set>
#include <utility>
#include <vector>

#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/mojom/constants.mojom.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "services/service_manager/public/cpp/connector.h"

class ChromeKeyboardControllerClientTestHelper::FakeKeyboardController
    : public ash::KeyboardController {
 public:
  FakeKeyboardController() = default;
  ~FakeKeyboardController() override = default;

  // ash::KeyboardController:
  keyboard::KeyboardConfig GetKeyboardConfig() override {
    return keyboard_config_;
  }
  void SetKeyboardConfig(
      const keyboard::KeyboardConfig& keyboard_config) override {
    keyboard_config_ = keyboard_config;
  }
  bool IsKeyboardEnabled() override { return enabled_; }
  void SetEnableFlag(keyboard::KeyboardEnableFlag flag) override {
    keyboard_enable_flags_.insert(flag);
    for (auto& observer : observers_)
      observer.OnKeyboardEnableFlagsChanged(keyboard_enable_flags_);
  }
  void ClearEnableFlag(keyboard::KeyboardEnableFlag flag) override {
    keyboard_enable_flags_.erase(flag);
    for (auto& observer : observers_)
      observer.OnKeyboardEnableFlagsChanged(keyboard_enable_flags_);
  }
  const std::set<keyboard::KeyboardEnableFlag>& GetEnableFlags() override {
    return keyboard_enable_flags_;
  }
  void ReloadKeyboardIfNeeded() override {}
  void RebuildKeyboardIfEnabled() override {}
  bool IsKeyboardVisible() override { return visible_; }
  void ShowKeyboard() override { visible_ = true; }
  void HideKeyboard(ash::HideReason reason) override { visible_ = false; }
  void SetContainerType(keyboard::ContainerType container_type,
                        const base::Optional<gfx::Rect>& target_bounds,
                        SetContainerTypeCallback callback) override {
    std::move(callback).Run(true);
  }
  void SetKeyboardLocked(bool locked) override {}
  void SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override {}
  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override {}
  void SetDraggableArea(const gfx::Rect& bounds) override {}
  bool SetAreaToRemainOnScreen(const gfx::Rect& bounds) override {
    return false;
  }
  void AddObserver(ash::KeyboardControllerObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(ash::KeyboardControllerObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

 private:
  keyboard::KeyboardConfig keyboard_config_;
  std::set<keyboard::KeyboardEnableFlag> keyboard_enable_flags_;
  bool enabled_ = false;
  bool visible_ = false;
  base::ObserverList<ash::KeyboardControllerObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeKeyboardController);
};

// static
std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
ChromeKeyboardControllerClientTestHelper::InitializeForAsh() {
  auto helper = std::make_unique<ChromeKeyboardControllerClientTestHelper>();
  helper->Initialize(ash::KeyboardController::Get());
  return helper;
}

// static
std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
ChromeKeyboardControllerClientTestHelper::InitializeWithFake() {
  auto helper = std::make_unique<ChromeKeyboardControllerClientTestHelper>();
  helper->fake_controller_ = std::make_unique<FakeKeyboardController>();
  helper->Initialize(helper->fake_controller_.get());
  return helper;
}

void ChromeKeyboardControllerClientTestHelper::Initialize(
    ash::KeyboardController* keyboard_controller) {
  chrome_keyboard_controller_client_ =
      ChromeKeyboardControllerClient::CreateForTest();
  chrome_keyboard_controller_client_->Init(keyboard_controller);
}

ChromeKeyboardControllerClientTestHelper::
    ChromeKeyboardControllerClientTestHelper() = default;

ChromeKeyboardControllerClientTestHelper::
    ~ChromeKeyboardControllerClientTestHelper() {
  chrome_keyboard_controller_client_.reset();
}

void ChromeKeyboardControllerClientTestHelper::SetProfile(Profile* profile) {
  chrome_keyboard_controller_client_->set_profile_for_test(profile);
}
