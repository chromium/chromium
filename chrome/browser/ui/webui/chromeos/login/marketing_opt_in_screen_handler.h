// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MARKETING_OPT_IN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MARKETING_OPT_IN_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class MarketingOptInScreen;

// Interface for dependency injection between MarketingOptInScreen and its
// WebUI representation.
class MarketingOptInScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"marketing-opt-in"};

  virtual ~MarketingOptInScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(MarketingOptInScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show(bool opt_in_visible,
                    bool opt_in_default_state,
                    bool legal_footer_visible) = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Sets whether the a11y Settings button is visible.
  virtual void UpdateA11ySettingsButtonVisibility(bool shown) = 0;

  // Sets whether the a11y setting for showing shelf navigation buttons is.
  // toggled on or off.
  virtual void UpdateA11yShelfNavigationButtonToggle(bool enabled) = 0;

};

// The sole implementation of the MarketingOptInScreenView, using WebUI.
class MarketingOptInScreenHandler : public BaseScreenHandler,
                                    public MarketingOptInScreenView {
 public:
  using TView = MarketingOptInScreenView;

  explicit MarketingOptInScreenHandler(JSCallsContainer* js_calls_container);
  ~MarketingOptInScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // MarketingOptInScreenView:
  void Bind(MarketingOptInScreen* screen) override;
  void Show(bool opt_in_visible,
            bool opt_in_default_state,
            bool legal_footer_visible) override;
  void Hide() override;
  void UpdateA11ySettingsButtonVisibility(bool shown) override;
  void UpdateA11yShelfNavigationButtonToggle(bool enabled) override;

 private:
  // BaseScreenHandler:
  void Initialize() override;
  void RegisterMessages() override;
  void GetAdditionalParameters(base::DictionaryValue* parameters) override;

  // WebUI event handlers.
  void HandleOnGetStarted(bool chromebook_email_opt_in);
  void HandleSetA11yNavigationButtonsEnabled(bool enabled);

  MarketingOptInScreen* screen_ = nullptr;

  // Timer to record user changed value for the accessibility setting to turn
  // shelf navigation buttons on in tablet mode. The metric is recorded with 10
  // second delay to avoid overreporting when the user keeps toggling the
  // setting value in the screen UI.
  base::OneShotTimer a11y_nav_buttons_toggle_metrics_reporter_timer_;

  DISALLOW_COPY_AND_ASSIGN(MarketingOptInScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MARKETING_OPT_IN_SCREEN_HANDLER_H_
