// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_buildflags.h"

class Browser;
struct CoreAccountId;
enum class SyncConfirmationStyle;

namespace content {
class WebContents;
}

namespace signin_metrics {
enum class ReauthAccessPoint;
}

// Interface to the platform-specific managers of the Signin and Sync
// confirmation tab-modal dialogs. This and its platform-specific
// implementations are responsible for actually creating and owning the dialogs,
// as well as managing the navigation inside them.
// Subclasses are responsible for deleting themselves when the window they're
// managing closes.
// TODO(crbug.com/40209493): rename to SigninModalDialogDelegate.
class SigninViewControllerDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a dialog controlled by this SigninViewControllerDelegate is
    // closed.
    virtual void OnModalDialogClosed() = 0;
  };

  SigninViewControllerDelegate(const SigninViewControllerDelegate&) = delete;
  SigninViewControllerDelegate& operator=(const SigninViewControllerDelegate&) =
      delete;

  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the sync confirmation dialog. The returned object should delete
  // itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateSyncConfirmationDelegate(
      Browser* browser,
      SyncConfirmationStyle style,
      bool is_sync_promo);

  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the modal sign in error dialog. The returned object should delete
  // itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateSigninErrorDelegate(
      Browser* browser);

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns a platform-specific SigninViewContolllerDelegate instance that
  // displays the reauth confirmation modal dialog. The returned object should
  // delete itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateReauthConfirmationDelegate(
      Browser* browser,
      const CoreAccountId& account_id,
      signin_metrics::ReauthAccessPoint access_point);

  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the profile customization modal dialog. The returned object should
  // delete itself when the window it's managing is closed.
  // If |is_local_profile_creation| is true, the profile customization will
  // display the local profile creation version of the page.
  // If |show_profile_switch_iph| is true, shows a profile switch IPH after the
  // user completes the profile customization.
  static SigninViewControllerDelegate* CreateProfileCustomizationDelegate(
      Browser* browser,
      bool is_local_profile_creation,
      bool show_profile_switch_iph = false);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns a platform-specific SigninViewContolllerDelegate instance that
  // displays the managed user notice modal dialog. The returned object
  // should delete itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateManagedUserNoticeDelegate(
      Browser* browser,
      std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
          create_param);
#endif

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Closes the sign-in dialog. Note that this method may destroy this object,
  // so the caller should no longer use this object after calling this method.
  virtual void CloseModalSignin() = 0;

  // This will be called by the base class to request a resize of the native
  // view hosting the content to |height|. |height| is the total height of the
  // content, in pixels.
  virtual void ResizeNativeView(int height) = 0;

  // Returns the web contents of the modal dialog.
  virtual content::WebContents* GetWebContents() = 0;

  // Overrides currently displayed WebContents with |web_contents|.
  virtual void SetWebContents(content::WebContents* web_contents) = 0;

 protected:
  SigninViewControllerDelegate();
  virtual ~SigninViewControllerDelegate();

  void NotifyModalDialogClosed();

 private:
  base::ObserverList<Observer, true> observer_list_;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
