// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_PROFILE_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_PROFILE_DIALOG_DELEGATE_H_

#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/user_manager.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

class GURL;
class UserManagerProfileDialogHost;

namespace views {
class WebView;
class View;
}  // namespace views

namespace gfx {
class Point;
class Size;
}  // namespace gfx

namespace web_modal {
class ModalDialogHostObserver;
}

class UserManagerProfileDialogDelegate
    : public views::DialogDelegateView,
      public UserManagerProfileDialog::BaseDialogDelegate,
      public ChromeWebModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  UserManagerProfileDialogDelegate(UserManagerProfileDialogHost* host,
                                   std::unique_ptr<views::WebView> web_view,
                                   const GURL& url);
  ~UserManagerProfileDialogDelegate() override;

  UserManagerProfileDialogDelegate(const UserManagerProfileDialogDelegate&) =
      delete;
  UserManagerProfileDialogDelegate& operator=(
      const UserManagerProfileDialogDelegate&) = delete;

  // UserManagerProfileDialog::BaseDialogDelegate
  void CloseDialog() override;

  // Display the local error message inside login window.
  void DisplayErrorMessage();

  // ChromeWebModalDialogManagerDelegate
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 private:
  // Before its destruction, tells its parent container to reset its reference
  // to the UserManagerProfileDialogDelegate.
  void OnDialogDestroyed();

  // views::DialogDelegate:
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  void DeleteDelegate() override;
  views::View* GetInitiallyFocusedView() override;

  UserManagerProfileDialogHost* host_;  // Not owned.
  views::WebView* web_view_;  // Owned by the view hierarchy.
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_PROFILE_DIALOG_DELEGATE_H_
