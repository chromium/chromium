// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ACCOUNT_SELECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ACCOUNT_SELECTION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class AccountSelectionScreen;

// Interface for dependency injection between AccountSelectionScreen and its
// WebUI representation.
class AccountSelectionScreenView {
 public:
  // LINT.IfChange(UsageMetrics)
  inline constexpr static StaticOobeScreenId kScreenId{
      "account-selection", "AccountSelectionScreen"};
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)

  virtual ~AccountSelectionScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  virtual void ShowStepProgress() = 0;
  virtual void SetUserEmail(const std::string& email) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<AccountSelectionScreenView> AsWeakPtr() = 0;
};

class AccountSelectionScreenHandler : public BaseScreenHandler,
                                      public AccountSelectionScreenView {
 public:
  using TView = AccountSelectionScreenView;

  AccountSelectionScreenHandler();

  AccountSelectionScreenHandler(const AccountSelectionScreenHandler&) = delete;
  AccountSelectionScreenHandler& operator=(
      const AccountSelectionScreenHandler&) = delete;

  ~AccountSelectionScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // AccountSelectionScreenView:
  void Show() override;

  // Shows the loading spinner.
  void ShowStepProgress() override;
  // Sets the email address to be displayed on the reuse account button.
  void SetUserEmail(const std::string& email) override;
  base::WeakPtr<AccountSelectionScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<AccountSelectionScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ACCOUNT_SELECTION_SCREEN_HANDLER_H_
