// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_COORDINATOR_H_

#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"

class Browser;
class ExtensionsMenuViewController;
class ExtensionsContainer;

namespace views {
class BubbleDialogDelegate;
class Widget;
}

// Handles the lifetime and showing/hidden state of the extensions menu bubble.
class ExtensionsMenuCoordinator : public views::ViewObserver {
 public:
  explicit ExtensionsMenuCoordinator(Browser* browser);
  ExtensionsMenuCoordinator(const ExtensionsMenuCoordinator&) = delete;
  const ExtensionsMenuCoordinator& operator=(const ExtensionsMenuCoordinator&) =
      delete;
  ~ExtensionsMenuCoordinator() override;

  // Displays the extensions menu under `anchor_view`.
  void Show(views::View* anchor_view,
            ExtensionsContainer* extensions_container);

  // Hides the currently-showing extensions menu, if it exists.
  void Hide();

  // Returns true if the extensions menu is showing.
  bool IsShowing() const;

  // Returns the currently-showing extensions menu widget, if it exists.
  views::Widget* GetExtensionsMenuWidget();

  // Accessors used by tests:
  ExtensionsMenuViewController* GetControllerForTesting() {
    return controller_.get();
  }
  std::unique_ptr<views::BubbleDialogDelegate>
  CreateExtensionsMenuBubbleDialogDelegateForTesting(
      views::View* anchor_view,
      ExtensionsContainer* extensions_container);

 private:
  // Creates the bubble contents and returns its delegate.
  std::unique_ptr<views::BubbleDialogDelegate>
  CreateExtensionsMenuBubbleDialogDelegate(
      views::View* anchor_view,
      ExtensionsContainer* extensions_container);

  // views::ViewObserver
  void OnViewIsDeleting(views::View* observed_view) override;

  const raw_ptr<Browser> browser_;
  views::ViewTracker bubble_tracker_;

  std::unique_ptr<ExtensionsMenuViewController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_COORDINATOR_H_
