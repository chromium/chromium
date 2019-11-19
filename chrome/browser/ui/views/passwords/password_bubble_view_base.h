// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_BUBBLE_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_BUBBLE_VIEW_BASE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

// Base class for all manage-passwords bubbles. Provides static methods for
// creating and showing these dialogs. Also used to access the web contents
// related to the dialog.
// These bubbles remove themselves as globals on destruction.
// TODO(pbos): Remove static global usage and move dialog ownership to
// TabDialog instances. Consider removing access to GetWebContents() through
// this class when ownership has moved to TabDialog instances, as it's hopefully
// no longer relevant for checking dialog ownership. These two work items should
// make this base class significantly smaller.
class PasswordBubbleViewBase : public LocationBarBubbleDelegateView {
 public:
  // Returns a pointer to the bubble.
  static PasswordBubbleViewBase* manage_password_bubble() {
    return g_manage_passwords_bubble_;
  }

  // Shows an appropriate bubble on the toolkit-views Browser window containing
  // |web_contents|.
  static void ShowBubble(content::WebContents* web_contents,
                         DisplayReason reason);
  // Creates and returns the passwords manager bubble UI appropriate for the
  // current password_manager::ui::State value for the provided |web_contents|.
  static PasswordBubbleViewBase* CreateBubble(
      content::WebContents* web_contents,
      views::View* anchor_view,
      DisplayReason reason);

  // Closes the existing bubble.
  static void CloseCurrentBubble();

  // Makes the bubble the foreground window.
  static void ActivateBubble();

  const content::WebContents* GetWebContents() const;

  // LocationBarBubbleDelegateView:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;

  // These model-accessor methods are public for testing.
  ManagePasswordsBubbleModel* model() { return &model_; }
  const ManagePasswordsBubbleModel* model() const { return &model_; }

 protected:
  // The |easilty_dismissable| flag indicates if the bubble should close upon
  // a click in the content area of the browser.
  PasswordBubbleViewBase(content::WebContents* web_contents,
                         views::View* anchor_view,
                         DisplayReason reason,
                         bool easily_dismissable);

  ~PasswordBubbleViewBase() override;

 private:
  // WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // Singleton instance of the Password bubble.The instance is owned by the
  // Bubble and will be deleted when the bubble closes.
  static PasswordBubbleViewBase* g_manage_passwords_bubble_;

  ManagePasswordsBubbleModel model_;

  // Listens for WebContentsView events and closes the bubble so the bubble gets
  // dismissed when users keep using the web page.
  std::unique_ptr<WebContentMouseHandler> mouse_handler_;

  DISALLOW_COPY_AND_ASSIGN(PasswordBubbleViewBase);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_BUBBLE_VIEW_BASE_H_
