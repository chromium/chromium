// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_REMOVE_LOCAL_AUTH_FACTORS_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_REMOVE_LOCAL_AUTH_FACTORS_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class RemoveLocalAuthFactorsScreen;

// Interface for dependency injection between RemoveLocalAuthFactorsScreen and
// its WebUI representation.
class RemoveLocalAuthFactorsScreenView {
 public:
  // LINT.IfChange(UsageMetrics)
  inline constexpr static StaticOobeScreenId kScreenId{
      "remove-local-auth-factors", "RemoveLocalAuthFactorsScreen"};
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)

  virtual ~RemoveLocalAuthFactorsScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(const std::string& email) = 0;

  // Shows the success step of the screen.
  virtual void ShowRemoveLocalAuthFactorsSuccessStep() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<RemoveLocalAuthFactorsScreenView> AsWeakPtr() = 0;
};

class RemoveLocalAuthFactorsScreenHandler
    : public BaseScreenHandler,
      public RemoveLocalAuthFactorsScreenView {
 public:
  using TView = RemoveLocalAuthFactorsScreenView;

  RemoveLocalAuthFactorsScreenHandler();

  RemoveLocalAuthFactorsScreenHandler(
      const RemoveLocalAuthFactorsScreenHandler&) = delete;
  RemoveLocalAuthFactorsScreenHandler& operator=(
      const RemoveLocalAuthFactorsScreenHandler&) = delete;

  ~RemoveLocalAuthFactorsScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // RemoveLocalAuthFactorsScreenView:
  void Show(const std::string& email) override;
  void ShowRemoveLocalAuthFactorsSuccessStep() override;
  base::WeakPtr<RemoveLocalAuthFactorsScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<RemoveLocalAuthFactorsScreenView> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_REMOVE_LOCAL_AUTH_FACTORS_SCREEN_HANDLER_H_
