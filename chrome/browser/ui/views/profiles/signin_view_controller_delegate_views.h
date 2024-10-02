// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
class GURL;
enum class SyncConfirmationStyle;

namespace content {
class WebContents;
class WebContentsDelegate;
}

namespace signin_metrics {
enum class ReauthAccessPoint;
}

namespace views {
class WebView;
}

// Views implementation of SigninViewControllerDelegate. It's responsible for
// managing the Signin and Sync Confirmation tab-modal dialogs.
// Instances of this class delete themselves when the window they're managing
// closes (in the DeleteDelegate callback).
class SigninViewControllerDelegateViews
    : public views::DialogDelegateView,
      public SigninViewControllerDelegate,
      public content::WebContentsDelegate,
      public ChromeWebModalDialogManagerDelegate {
  METADATA_HEADER(SigninViewControllerDelegateViews, views::DialogDelegateView)

 public:
  SigninViewControllerDelegateViews(const SigninViewControllerDelegateViews&) =
      delete;
  SigninViewControllerDelegateViews& operator=(
      const SigninViewControllerDelegateViews&) = delete;

  static std::unique_ptr<views::WebView> CreateSyncConfirmationWebView(
      Browser* browser,
      SyncConfirmationStyle style,
      bool is_sync_promo);

  static std::unique_ptr<views::WebView> CreateSigninErrorWebView(
      Browser* browser);

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  static std::unique_ptr<views::WebView> CreateReauthConfirmationWebView(
      Browser* browser,
      signin_metrics::ReauthAccessPoint);

  static std::unique_ptr<views::WebView> CreateProfileCustomizationWebView(
      Browser* browser,
      bool is_local_profile_creation,
      bool show_profile_switch_iph = false);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  static std::unique_ptr<views::WebView>
  CreateManagedUserNoticeConfirmationWebView(
      Browser* browser,
      std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
          create_param);
#endif

  // views::DialogDelegateView:
  bool ShouldShowCloseButton() const override;

  // SigninViewControllerDelegate:
  void CloseModalSignin() override;
  void ResizeNativeView(int height) override;
  content::WebContents* GetWebContents() override;
  void SetWebContents(content::WebContents* web_contents) override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

 private:
  friend SigninViewControllerDelegate;
  friend class SigninViewControllerDelegateViewsBrowserTest;

  using InitializeSigninWebDialogUI =
      base::StrongAlias<class InitializeSigninWebDialogUITag, bool>;

  // Creates and displays a constrained window containing |web_contents|. If
  // |wait_for_size| is true, the delegate will wait for ResizeNativeView() to
  // be called by the base class before displaying the constrained window.
  SigninViewControllerDelegateViews(
      std::unique_ptr<views::WebView> content_view,
      Browser* browser,
      ui::mojom::ModalType dialog_modal_type,
      bool wait_for_size,
      bool should_show_close_button,
      bool delete_profile_on_cancel = false);
  ~SigninViewControllerDelegateViews() override;

  // Creates a WebView for a dialog with the specified URL.
  static std::unique_ptr<views::WebView> CreateDialogWebView(
      Browser* browser,
      const GURL& url,
      int dialog_height,
      std::optional<int> dialog_width,
      InitializeSigninWebDialogUI initialize_signin_web_dialog_ui);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  // Deletes the ephemeral profile when cancelling the local profile creation
  // dialog.
  void DeleteProfileOnCancel();
#endif

  // Displays the modal dialog.
  void DisplayModal();

  // If the widget is non-null, then it owns the
  // `SigninViewControllerDelegateViews` and the content view.
  raw_ptr<views::Widget> modal_signin_widget_ = nullptr;

  const raw_ptr<views::WebView> content_view_;
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;
  const raw_ptr<Browser> browser_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  bool should_show_close_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
