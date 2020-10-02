// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "ui/views/view.h"

class ToolbarView;
enum class InProductHelpFeature;

// The app menu button in the main browser window (as opposed to web app
// windows, which is implemented in WebAppMenuButton).
class BrowserAppMenuButton : public AppMenuButton {
 public:
  BrowserAppMenuButton(PressedCallback callback, ToolbarView* toolbar_view);
  BrowserAppMenuButton(const BrowserAppMenuButton&) = delete;
  BrowserAppMenuButton& operator=(const BrowserAppMenuButton&) = delete;
  ~BrowserAppMenuButton() override;

  void SetTypeAndSeverity(
      AppMenuIconController::TypeAndSeverity type_and_severity);

  AppMenuIconController::Severity severity() {
    return type_and_severity_.severity;
  }

  // Shows the app menu. |run_types| denotes the MenuRunner::RunTypes associated
  // with the menu.
  void ShowMenu(int run_types);

  // Called to inform the button that it's being used as an anchor for a promo
  // for |promo_feature|.  When this is non-null, the button is highlighted in a
  // noticeable color, and the menu item appearance may be affected.
  void SetPromoFeature(base::Optional<InProductHelpFeature> promo_feature);

  // Opens the app menu immediately during a drag-and-drop operation.
  // Used only in testing.
  static bool g_open_app_immediately_for_testing;

  // AppMenuButton:
  const char* GetClassName() const override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  SkColor GetInkDropBaseColor() const override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;
  void OnThemeChanged() override;
  // Updates the presentation according to |severity_| and the theme provider.
  void UpdateIcon() override;

 protected:
  // If the button is being used as an anchor for a promo, returns the best
  // promo color given the current background color. Otherwise, returns the
  // standard ToolbarButton foreground color for the given |state|.
  SkColor GetForegroundColor(ButtonState state) const override;

 private:
  void OnTouchUiChanged();

  void UpdateTextAndHighlightColor();

  AppMenuIconController::TypeAndSeverity type_and_severity_{
      AppMenuIconController::IconType::NONE,
      AppMenuIconController::Severity::NONE};

  // Our owning toolbar view.
  ToolbarView* const toolbar_view_;

  // The feature, if any, for which this button is anchoring a promo.
  base::Optional<InProductHelpFeature> promo_feature_;

  std::unique_ptr<ui::TouchUiController::Subscription> subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&BrowserAppMenuButton::OnTouchUiChanged,
                              base::Unretained(this)));

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<BrowserAppMenuButton> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_
