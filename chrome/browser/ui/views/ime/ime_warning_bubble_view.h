// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_IME_WARNING_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IME_IME_WARNING_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class BrowserActionsContainer;
class BrowserView;
enum class ImeWarningBubblePermissionStatus;

namespace extensions {
class Extension;
}

namespace views {
class Checkbox;
}

using ImeWarningBubbleResponseCallback =
    base::Callback<void(ImeWarningBubblePermissionStatus status)>;

// The implementation for the IME warning bubble. Provides warning information
// to the user upon the activation of an IME extension.
// The ImeWarningBubbleView is self-owned and deletes itself when it is closed
// or the parent browser is being destroyed.
class ImeWarningBubbleView : public views::BubbleDialogDelegateView,
                             public ToolbarActionsBarObserver,
                             public BrowserListObserver {
 public:
  static void ShowBubble(const extensions::Extension* extension,
                         BrowserView* browser_view,
                         const ImeWarningBubbleResponseCallback& callback);

  // views::DialogDelegate:
  bool Accept() override;
  bool Cancel() override;

  // ToolbarActionsBarObserver:
  void OnToolbarActionsBarAnimationEnded() override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  friend class ImeWarningBubbleTest;

  static ImeWarningBubbleView* ime_warning_bubble_for_test_;

  ImeWarningBubbleView(const extensions::Extension* extension,
                       BrowserView* browser_view,
                       const ImeWarningBubbleResponseCallback& callback);

  ~ImeWarningBubbleView() override;

  // Recalculates the anchor position for this bubble.
  void InitAnchorView();

  // Intializes the layout for the bubble.
  void InitLayout();

  // Returns true if the action toolbar is animating.
  bool IsToolbarAnimating();

  const extensions::Extension* extension_;
  BrowserView* browser_view_;

  // Saves the Browser instance of the browser view, which will be used in
  // OnBrowserRemoved(), as browser_view_->browser() may be null when
  // OnBrowserRemoved() is called.
  Browser* const browser_;

  // True if bubble anchors to the action of the extension.
  bool anchor_to_action_ = false;

  // The check box on the bubble view.
  views::Checkbox* never_show_checkbox_ = nullptr;

  ImeWarningBubbleResponseCallback response_callback_;

  // True if the warning bubble has been shown.
  bool bubble_has_shown_ = false;

  BrowserActionsContainer* container_;

  ToolbarActionsBar* toolbar_actions_bar_;

  ScopedObserver<ToolbarActionsBar, ToolbarActionsBarObserver>
      toolbar_actions_bar_observer_{this};

  base::WeakPtrFactory<ImeWarningBubbleView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImeWarningBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_IME_WARNING_BUBBLE_VIEW_H_
