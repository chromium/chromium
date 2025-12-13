// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"

class Browser;
class ExtensionsMenuViewPlatformDelegateViews;
class ExtensionsMenuViewModel;
class ExtensionsContainerViews;

namespace views {
class BubbleDialogDelegate;
class Widget;
}  // namespace views

// Handles the lifetime and showing/hidden state of the extensions menu bubble.
class ExtensionsMenuCoordinator : public views::ViewObserver {
 public:
  explicit ExtensionsMenuCoordinator(Browser* browser);
  ExtensionsMenuCoordinator(const ExtensionsMenuCoordinator&) = delete;
  const ExtensionsMenuCoordinator& operator=(const ExtensionsMenuCoordinator&) =
      delete;
  ~ExtensionsMenuCoordinator() override;

  // Displays the extensions menu under `anchor`.
  void Show(views::BubbleAnchor anchor,
            ExtensionsContainerViews* extensions_container);

  // Hides the currently-showing extensions menu, if it exists.
  void Hide();

  // Returns true if the extensions menu is showing.
  bool IsShowing() const;

  // Returns the currently-showing extensions menu widget, if it exists.
  views::Widget* GetExtensionsMenuWidget();

  // Accessors used by tests:
  ExtensionsMenuViewPlatformDelegateViews* GetDelegateForTesting() {
    return menu_delegate_;
  }
  std::unique_ptr<views::BubbleDialogDelegate>
  CreateExtensionsMenuBubbleDialogDelegateForTesting(
      views::BubbleAnchor anchor,
      ExtensionsContainerViews* extensions_container);

 private:
  // Creates the bubble contents and returns its delegate.
  std::unique_ptr<views::BubbleDialogDelegate>
  CreateExtensionsMenuBubbleDialogDelegate(
      views::BubbleAnchor anchor,
      ExtensionsContainerViews* extensions_container);

  // views::ViewObserver
  void OnViewIsDeleting(views::View* observed_view) override;

  const raw_ptr<Browser> browser_;
  views::ViewTracker bubble_tracker_;

  base::ScopedObservation<views::View, views::ViewObserver>
      bubble_view_observation_{this};

  // The model for the extensions menu.
  std::unique_ptr<ExtensionsMenuViewModel> menu_model_;

  // The platform delegate for the extensions menu.
  raw_ptr<ExtensionsMenuViewPlatformDelegateViews> menu_delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_COORDINATOR_H_
