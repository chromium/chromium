// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FORCE_SIGNIN_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FORCE_SIGNIN_DIALOG_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

class GURL;
class ProfilePickerForceSigninDialogHost;

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

class ProfilePickerForceSigninDialogDelegate
    : public views::DialogDelegateView,
      public content::WebContentsDelegate,
      public ChromeWebModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
  METADATA_HEADER(ProfilePickerForceSigninDialogDelegate,
                  views::DialogDelegateView)

 public:
  ProfilePickerForceSigninDialogDelegate(
      ProfilePickerForceSigninDialogHost* host,
      std::unique_ptr<views::WebView> web_view,
      const GURL& url);
  ProfilePickerForceSigninDialogDelegate(
      const ProfilePickerForceSigninDialogDelegate&) = delete;
  ProfilePickerForceSigninDialogDelegate& operator=(
      const ProfilePickerForceSigninDialogDelegate&) = delete;
  ~ProfilePickerForceSigninDialogDelegate() override;

  void CloseDialog();

  // Display the local error message inside login window.
  void DisplayErrorMessage();

  // content::WebContentsDelegate
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  // ChromeWebModalDialogManagerDelegate
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  content::WebContents* GetWebContentsForTesting() const;

 private:
  // Before its destruction, tells its parent container to reset its reference
  // to the ProfilePickerForceSigninDialogDelegate.
  void OnDialogDestroyed();

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  views::View* GetInitiallyFocusedView() override;

  raw_ptr<ProfilePickerForceSigninDialogHost> host_;  // Not owned.
  raw_ptr<views::WebView> web_view_;  // Owned by the view hierarchy.
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FORCE_SIGNIN_DIALOG_DELEGATE_H_
