// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SUPERVISION_TRANSITION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SUPERVISION_TRANSITION_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "components/prefs/pref_change_registrar.h"

namespace chromeos {

class SupervisionTransitionScreen;

// Interface for dependency injection between SupervisionTransitionScreen
// and its WebUI representation.
class SupervisionTransitionScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"supervision-transition"};

  virtual ~SupervisionTransitionScreenView() {}

  virtual void Bind(SupervisionTransitionScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

 protected:
  SupervisionTransitionScreenView() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisionTransitionScreenView);
};

class SupervisionTransitionScreenHandler
    : public BaseScreenHandler,
      public SupervisionTransitionScreenView {
 public:
  using TView = SupervisionTransitionScreenView;

  explicit SupervisionTransitionScreenHandler(
      JSCallsContainer* js_calls_container);
  ~SupervisionTransitionScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;

  // SupervisionTransitionScreenView:
  void Bind(SupervisionTransitionScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

  base::OneShotTimer* GetTimerForTesting();

 private:
  // BaseScreenHandler:
  void Initialize() override;

  // Called when the max wait timeout is reached.
  void OnSupervisionTransitionFailed();

  void OnSupervisionTransitionFinished();

  SupervisionTransitionScreen* screen_ = nullptr;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_ = false;

  // Whether screen timed out waiting for transition to occur and displayed the
  // error screen.
  bool timed_out_ = false;

  base::TimeTicks screen_shown_time_;

  // The primary user profile.
  Profile* profile_ = nullptr;

  // Timer used to exit the page when timeout reaches.
  base::OneShotTimer timer_;

  // Listens to pref changes.
  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<SupervisionTransitionScreenHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SupervisionTransitionScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SUPERVISION_TRANSITION_SCREEN_HANDLER_H_
