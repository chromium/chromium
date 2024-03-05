// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_MARKETING_OPT_IN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_MARKETING_OPT_IN_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between MarketingOptInScreen and its
// WebUI representation.
class MarketingOptInScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"marketing-opt-in",
                                                       "MarketingOptInScreen"};

  virtual ~MarketingOptInScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(bool opt_in_visible,
                    bool opt_in_default_state,
                    bool legal_footer_visible,
                    bool cloud_gaming_enabled) = 0;

  // Sets whether the a11y Settings button is visible.
  virtual void UpdateA11ySettingsButtonVisibility(bool shown) = 0;

  // Sets whether the a11y setting for showing shelf navigation buttons is.
  // toggled on or off.
  virtual void UpdateA11yShelfNavigationButtonToggle(bool enabled) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<MarketingOptInScreenView> AsWeakPtr() = 0;
};

// The sole implementation of the MarketingOptInScreenView, using WebUI.
class MarketingOptInScreenHandler final : public BaseScreenHandler,
                                          public MarketingOptInScreenView {
 public:
  using TView = MarketingOptInScreenView;

  MarketingOptInScreenHandler();

  MarketingOptInScreenHandler(const MarketingOptInScreenHandler&) = delete;
  MarketingOptInScreenHandler& operator=(const MarketingOptInScreenHandler&) =
      delete;

  ~MarketingOptInScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // MarketingOptInScreenView:
  void Show(bool opt_in_visible,
            bool opt_in_default_state,
            bool legal_footer_visible,
            bool cloud_gaming_enabled) override;
  void UpdateA11ySettingsButtonVisibility(bool shown) override;
  void UpdateA11yShelfNavigationButtonToggle(bool enabled) override;
  base::WeakPtr<MarketingOptInScreenView> AsWeakPtr() override;

 private:
  // BaseScreenHandler:
  void GetAdditionalParameters(base::Value::Dict* parameters) override;

  base::WeakPtrFactory<MarketingOptInScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_MARKETING_OPT_IN_SCREEN_HANDLER_H_
