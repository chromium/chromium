// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_REAUTH_DIALOGS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_REAUTH_DIALOGS_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/base_lock_dialog.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chromeos/ash/components/http_auth_dialog/http_auth_dialog.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class LockScreenCaptivePortalDialog;
class LockScreenNetworkDialog;

class LockScreenStartReauthDialog
    : public BaseLockDialog,
      public NetworkStateInformer::NetworkStateInformerObserver,
      public ChromeWebModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost,
      public HttpAuthDialog::Observer {
 public:
  LockScreenStartReauthDialog();
  LockScreenStartReauthDialog(LockScreenStartReauthDialog const&) = delete;
  ~LockScreenStartReauthDialog() override;

  // content::WebDialogDelegate
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  // Creates singleton instance of LockScreenStartReauthDialog. It will
  // self-destruct in `OnDialogClosed` method.
  static void Show();
  static void Dismiss();
  static bool IsShown();
  static LockScreenStartReauthDialog* GetInstance();

  int GetDialogWidth() const;
  content::WebContents* GetWebContents();

  void DismissLockScreenNetworkDialog();
  void DismissLockScreenCaptivePortalDialog();
  void ShowLockScreenNetworkDialog();
  void ShowLockScreenCaptivePortalDialog();
  static gfx::Size CalculateLockScreenReauthDialogSize(
      bool is_new_layout_enabled);

  // Forces network state update because webview reported frame loading error.
  void OnWebviewLoadAborted();

  // Used for waiting for the corresponding dialogs in tests.
  // Similar methods exist for the main dialog in InSessionPasswordSyncManager.
  bool IsNetworkDialogLoadedForTesting(base::OnceClosure callback);
  bool IsCaptivePortalDialogLoadedForTesting(base::OnceClosure callback);
  void OnNetworkDialogReadyForTesting();

  // Check if dialog is loaded.
  // `callback` is used to notify test when the reauth dialog is loaded.
  bool IsLoadedForTesting(base::OnceClosure callback);

  // Check if dialog is closed.
  // `callback` is used to notify test when the reauth dialog is closed.
  bool IsClosedForTesting(base::OnceClosure callback);

  // Notify test that the dialog is ready for testing.
  void OnReadyForTesting();

  LockScreenNetworkDialog* get_network_dialog_for_testing() {
    return lock_screen_network_dialog_.get();
  }

  LockScreenCaptivePortalDialog* get_captive_portal_dialog_for_testing() {
    return captive_portal_dialog_.get();
  }

  bool is_network_dialog_visible_for_testing() {
    return is_network_dialog_visible_;
  }

 private:
  class ModalDialogManagerCleanup;

  void OnProfileInitialized(Profile* profile);
  void DeleteLockScreenNetworkDialog();

  // BaseLockDialog:
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

  // NetworkStateInformer::NetworkStateInformerObserver:
  void UpdateState(NetworkError::ErrorReason reason) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost:
  gfx::Size GetMaximumDialogSize() override;
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // HttpAuthDialog::Observer implementation:
  void HttpAuthDialogShown(content::WebContents* web_contents) override;
  void HttpAuthDialogCancelled(content::WebContents* web_contents) override;
  void HttpAuthDialogSupplied(content::WebContents* web_contents) override;

  // Returns whether `web_contents` is associated with this instance.
  bool Matches(content::WebContents* web_contents);

  // Copies proxy authentication details that were entered in the lock screen
  // profile to system network context and to the profile of active user.
  void TransferHttpAuthCaches();

  void ReenableNetworkUpdates();

  void OnCaptivePortalDialogReadyForTesting();

  scoped_refptr<NetworkStateInformer> network_state_informer_;
  bool is_network_dialog_visible_ = false;
  bool is_proxy_auth_in_progress_ = false;
  bool should_reload_gaia_ = false;

  base::ScopedObservation<NetworkStateInformer, NetworkStateInformerObserver>
      scoped_observation_{this};

  std::unique_ptr<LockScreenNetworkDialog> lock_screen_network_dialog_;
  raw_ptr<Profile> profile_ = nullptr;

  std::unique_ptr<LockScreenCaptivePortalDialog> captive_portal_dialog_;

  // Once Lacros is shipped, this will no longer be necessary.
  std::unique_ptr<HttpAuthDialog::ScopedEnabler> enable_ash_httpauth_;

  // Callbacks and flags that are used in tests to check that the corresponding
  // dialog is loaded or closed.
  base::OnceClosure on_dialog_loaded_callback_for_testing_;
  base::OnceClosure on_dialog_closed_callback_for_testing_;
  base::OnceClosure on_network_dialog_loaded_callback_for_testing_;
  base::OnceClosure on_captive_portal_dialog_loaded_callback_for_testing_;
  bool is_dialog_loaded_for_testing_ = false;
  bool is_network_dialog_loaded_for_testing_ = false;
  bool is_captive_portal_dialog_loaded_for_testing_ = false;

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observer_list_;
  std::unique_ptr<ModalDialogManagerCleanup> modal_dialog_manager_cleanup_;

  base::WeakPtrFactory<LockScreenStartReauthDialog> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_REAUTH_DIALOGS_H_
