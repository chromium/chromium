// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MANAGEMENT_TRANSITION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MANAGEMENT_TRANSITION_SCREEN_HANDLER_H_

#include "ash/components/arc/session/arc_management_transition.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// Interface for dependency injection between ManagementTransitionScreen
// and its WebUI representation.
class ManagementTransitionScreenView
    : public base::SupportsWeakPtr<ManagementTransitionScreenView> {
 public:
  // Renamed from "supervision-transition".
  inline constexpr static StaticOobeScreenId kScreenId{
      "management-transition", "ManagementTransitionScreen"};

  ManagementTransitionScreenView(const ManagementTransitionScreenView&) =
      delete;
  ManagementTransitionScreenView& operator=(
      const ManagementTransitionScreenView&) = delete;

  virtual ~ManagementTransitionScreenView() = default;

  virtual void Show(arc::ArcManagementTransition arc_management_transition,
                    std::string management_entity) = 0;

  virtual void ShowError() = 0;

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

  void Show(arc::ArcManagementTransition arc_management_transition,
            std::string management_entity) override;

  void ShowError() override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ManagementTransitionScreenHandler;
using ::chromeos::ManagementTransitionScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MANAGEMENT_TRANSITION_SCREEN_HANDLER_H_
