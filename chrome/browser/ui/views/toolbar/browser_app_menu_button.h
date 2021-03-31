// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class ToolbarView;
enum class InProductHelpFeature;

// The app menu button in the main browser window (as opposed to web app
// windows, which is implemented in WebAppMenuButton).
class BrowserAppMenuButton : public AppMenuButton {
 public:
  METADATA_HEADER(BrowserAppMenuButton);
  BrowserAppMenuButton(PressedCallback callback, ToolbarView* toolbar_view);
  BrowserAppMenuButton(const BrowserAppMenuButton&) = delete;
  BrowserAppMenuButton& operator=(const BrowserAppMenuButton&) = delete;
  ~BrowserAppMenuButton() override;

  void SetTypeAndSeverity(
      AppMenuIconController::TypeAndSeverity type_and_severity);

  // Shows the app menu. |run_types| denotes the MenuRunner::RunTypes associated
  // with the menu.
  void ShowMenu(int run_types);

  // Opens the app menu immediately during a drag-and-drop operation.
  // Used only in testing.
  static bool g_open_app_immediately_for_testing;

  // AppMenuButton:
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  void OnThemeChanged() override;
  // Updates the presentation according to |severity_| and the theme provider.
  void UpdateIcon() override;
  void HandleMenuClosed() override;

 private:
  void OnTouchUiChanged();

  void UpdateTextAndHighlightColor();

  void SetHasInProductHelpPromo(bool has_in_product_help_promo);

  AppMenuIconController::TypeAndSeverity type_and_severity_{
      AppMenuIconController::IconType::NONE,
      AppMenuIconController::Severity::NONE};

  // Our owning toolbar view.
  ToolbarView* const toolbar_view_;

  base::Optional<FeaturePromoController::PromoHandle> reopen_tab_promo_handle_;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&BrowserAppMenuButton::OnTouchUiChanged,
                              base::Unretained(this)));

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<BrowserAppMenuButton> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_
