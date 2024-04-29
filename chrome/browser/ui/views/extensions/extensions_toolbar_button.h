// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_chip_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class ExtensionsToolbarContainer;
class ExtensionsMenuCoordinator;

// Button in the toolbar that provides access to the corresponding extensions
// menu.
class ExtensionsToolbarButton : public ToolbarChipButton,
                                public views::WidgetObserver {
  METADATA_HEADER(ExtensionsToolbarButton, ToolbarChipButton)

 public:
  enum class State {
    // All extensions have blocked access to the current site.
    kAllExtensionsBlocked,
    // At least one extension has access to the current site.
    kAnyExtensionHasAccess,
    kDefault,
  };

  ExtensionsToolbarButton(Browser* browser,
                          ExtensionsToolbarContainer* extensions_container,
                          ExtensionsMenuCoordinator* coordinator);
  ExtensionsToolbarButton(const ExtensionsToolbarButton&) = delete;
  ExtensionsToolbarButton& operator=(const ExtensionsToolbarButton&) = delete;
  ~ExtensionsToolbarButton() override;

  // Toggle the Extensions menu. If the ExtensionsToolbarContainer is in
  // kAutoHide mode and hidden this will cause it to show.
  void ToggleExtensionsMenu();

  bool GetExtensionsMenuShowing() const;

  void UpdateState(State state);

  State state() { return state_; }

  // ToolbarButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool ShouldShowInkdropAfterIphInteraction() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  int GetIconSize() const override;

  // A lock to keep the button pressed when a popup is visible.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  const raw_ptr<Browser> browser_;
  raw_ptr<views::MenuButtonController> menu_button_controller_;
  const raw_ptr<ExtensionsToolbarContainer> extensions_container_;
  // This can be nullptr before `kExtensionsMenuAccessControl` feature is fully
  // rolled out.
  // TODO(crbug.com/40811196): Remove this disclaimer once feature is rolled
  // out.
  const raw_ptr<ExtensionsMenuCoordinator> extensions_menu_coordinator_;

  // The type for the button icon.
  State state_ = State::kDefault;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
