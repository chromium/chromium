// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MANAGEMENT_TRANSITION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MANAGEMENT_TRANSITION_SCREEN_HANDLER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {
class ManagementTransitionScreen;
}

namespace chromeos {

// Interface for dependency injection between ManagementTransitionScreen
// and its WebUI representation.
class ManagementTransitionScreenView {
 public:
  // Renamed from "supervision-transition".
  constexpr static StaticOobeScreenId kScreenId{"management-transition"};

  ManagementTransitionScreenView(const ManagementTransitionScreenView&) =
      delete;
  ManagementTransitionScreenView& operator=(
      const ManagementTransitionScreenView&) = delete;

  virtual ~ManagementTransitionScreenView() {}

  virtual void Bind(ash::ManagementTransitionScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

 protected:
  ManagementTransitionScreenView() = default;
};

class ManagementTransitionScreenHandler
    : public BaseScreenHandler,
      public ManagementTransitionScreenView {
 public:
  using TView = ManagementTransitionScreenView;

  ManagementTransitionScreenHandler();

  ManagementTransitionScreenHandler(const ManagementTransitionScreenHandler&) =
      delete;
  ManagementTransitionScreenHandler& operator=(
      const ManagementTransitionScreenHandler&) = delete;

  ~ManagementTransitionScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;

  // ManagementTransitionScreenView:
  void Bind(ash::ManagementTransitionScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

  base::OneShotTimer* GetTimerForTesting();

 private:
  // BaseScreenHandler:
  void InitializeDeprecated() override;

  // Shows a given step.
  void ShowStep(const char* step);

  // Called when the max wait timeout is reached.
  void OnManagementTransitionFailed();

  void OnManagementTransitionFinished();

  ash::ManagementTransitionScreen* screen_ = nullptr;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_ = false;

  // Whether screen timed out waiting for transition to occur and displayed the
  // error screen.
  bool timed_out_ = false;

  base::TimeTicks screen_shown_time_;

  // Timer used to exit the page when timeout reaches.
  base::OneShotTimer timer_;

  // Listens to pref changes.
  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<ManagementTransitionScreenHandler> weak_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ManagementTransitionScreenHandler;
using ::chromeos::ManagementTransitionScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MANAGEMENT_TRANSITION_SCREEN_HANDLER_H_
