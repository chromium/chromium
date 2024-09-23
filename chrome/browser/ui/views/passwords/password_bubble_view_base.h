// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_BUBBLE_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_BUBBLE_VIEW_BASE_H_

#include "build/build_config.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

class Browser;
class PasswordBubbleControllerBase;

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
  METADATA_HEADER(PasswordBubbleViewBase, LocationBarBubbleDelegateView)

 public:
  PasswordBubbleViewBase(const PasswordBubbleViewBase&) = delete;
  PasswordBubbleViewBase& operator=(const PasswordBubbleViewBase&) = delete;

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

  // Returns the PasswordBubbleController used by the view. Returns nullptr if
  // the view is still using the ManagerPasswordBubbleModel instead of a
  // PasswordBubbleController.
  virtual PasswordBubbleControllerBase* GetController() = 0;
  virtual const PasswordBubbleControllerBase* GetController() const = 0;

 protected:
  // The |easily_dismissable| flag indicates if the bubble should close upon
  // a click in the content area of the browser.
  PasswordBubbleViewBase(content::WebContents* web_contents,
                         views::View* anchor_view,
                         bool easily_dismissable);

  ~PasswordBubbleViewBase() override;

  // Sets the resource ids of the images used in the header in light and dark
  // mode.
  void SetBubbleHeader(int light_image_id, int dark_image_id);

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;

  raw_ptr<Browser> browser_ = nullptr;

  // Singleton instance of the Password bubble.The instance is owned by the
  // Bubble and will be deleted when the bubble closes.
  static PasswordBubbleViewBase* g_manage_passwords_bubble_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_BUBBLE_VIEW_BASE_H_
