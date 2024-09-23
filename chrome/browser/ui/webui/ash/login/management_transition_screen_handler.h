// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_MANAGEMENT_TRANSITION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_MANAGEMENT_TRANSITION_SCREEN_HANDLER_H_

#include "ash/components/arc/session/arc_management_transition.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between ManagementTransitionScreen
// and its WebUI representation.
class ManagementTransitionScreenView {
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

  virtual base::WeakPtr<ManagementTransitionScreenView> AsWeakPtr() = 0;

 protected:
  ManagementTransitionScreenView() = default;
};

class ManagementTransitionScreenHandler final
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

  base::WeakPtr<ManagementTransitionScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<ManagementTransitionScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_MANAGEMENT_TRANSITION_SCREEN_HANDLER_H_
