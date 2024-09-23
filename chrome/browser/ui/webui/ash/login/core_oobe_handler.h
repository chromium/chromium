// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CORE_OOBE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CORE_OOBE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "ui/events/event_source.h"

namespace ui {
class EventSink;
}

namespace ash {

class PriorityScreenChecker {
 public:
  static bool IsPriorityScreen(OobeScreenId screen_id) {
    for (const auto& priority_screen : priority_screens_) {
      if (screen_id == priority_screen) {
        return true;
      }
    }
    return false;
  }

 private:
  constexpr static StaticOobeScreenId priority_screens_[] = {
      WelcomeView::kScreenId, UserCreationView::kScreenId};
};

class CoreOobeView {
 public:
  // Possible Initialization States of the UI
  enum class UiState {
    kUninitialized,           // Start of things
    kCoreHandlerInitialized,  // First oobe.js instruction
    kPriorityScreensLoaded,   // Priority screens are loaded and could be shown.
    kFullyInitialized,        // Fully initialized a.k.a 'screenStateInitialize'
  };

  virtual ~CoreOobeView() = default;

  // The following methods must only be called once the UI is fully initialized
  // (kFullyInitialized). It is the responsibility of the client (CoreOobe) to
  // ensure that this invariant is met. Otherwise it will CHECK().
  virtual void ShowScreenWithData(const OobeScreenId& screen,
                                  std::optional<base::Value::Dict> data) = 0;
  virtual void UpdateOobeConfiguration() = 0;
  virtual void ReloadContent() = 0;
  virtual void ForwardCancel() = 0;
  virtual void SetTabletModeState(bool tablet_mode_enabled) = 0;

  // The following methods are safe to be called at any |UiState|, although the
  // existing mechanism inside |BaseWebUIHandler| will defer the calls until
  // JavaScript is allowed in this handler (kCoreHandlerInitialized)
  virtual void ToggleSystemInfo() = 0;
  virtual void TriggerDown() = 0;
  virtual void EnableKeyboardFlow() = 0;
  virtual void SetShelfHeight(int height) = 0;
  virtual void SetOrientation(bool is_horizontal) = 0;
  virtual void SetDialogSize(int width, int height) = 0;
  virtual void SetVirtualKeyboardShown(bool shown) = 0;
  virtual void SetOsVersionLabelText(const std::string& label_text) = 0;
  virtual void SetBluetoothDeviceInfo(const std::string& bluetooth_name) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<CoreOobeView> AsWeakPtr() = 0;
};

// The core handler for Javascript messages related to the "oobe" view.
class CoreOobeHandler final : public BaseWebUIHandler,
                              public CoreOobeView,
                              public ui::EventSource {
 public:
  CoreOobeHandler();
  CoreOobeHandler(const CoreOobeHandler&) = delete;
  CoreOobeHandler& operator=(const CoreOobeHandler&) = delete;
  ~CoreOobeHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;

  // CoreOobeView
  base::WeakPtr<CoreOobeView> AsWeakPtr() override;

 private:
  // ui::EventSource implementation:
  ui::EventSink* GetEventSink() override;

  // ---- BEGIN --- CoreOobeView
  void ShowScreenWithData(const OobeScreenId& screen,
                          std::optional<base::Value::Dict> data) override;
  void UpdateOobeConfiguration() override;
  void ReloadContent() override;
  void ForwardCancel() override;
  void SetTabletModeState(bool tablet_mode_enabled) override;
  // Calls above will CHECK() if not |kFullyInitialized|
  // Calls below are safe at any moment
  void ToggleSystemInfo() override;
  void TriggerDown() override;
  void EnableKeyboardFlow() override;
  void SetShelfHeight(int height) override;
  void SetOrientation(bool is_horizontal) override;
  void SetDialogSize(int width, int height) override;
  void SetVirtualKeyboardShown(bool shown) override;
  void SetOsVersionLabelText(const std::string& label_text) override;
  void SetBluetoothDeviceInfo(const std::string& bluetooth_name) override;
  // ---- END --- CoreOobeView

  // ---- Handlers for JS WebUI messages.
  void HandleInitializeCoreHandler();
  void HandlePrriorityScreensLoaded();
  void HandleScreenStateInitialize();
  void HandleEnableShelfButtons(bool enable);
  void HandleUpdateCurrentScreen(const std::string& screen);
  void HandleBackdropLoaded();
  void HandleLaunchHelpApp(int help_topic_id);
  // Handles demo mode setup for tests. Accepts 'online' and 'offline' as
  // `demo_config`.
  void HandleUpdateOobeUIState(int state);
  // When keyboard_utils.js arrow key down event is reached, raise it
  // to tab/shift-tab event.
  void HandleRaiseTabKeyEvent(bool reverse);

  // Initialization state that is kept in sync with |CoreOobe|.
  UiState ui_init_state_ = UiState::kUninitialized;

  base::WeakPtrFactory<CoreOobeView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CORE_OOBE_HANDLER_H_
