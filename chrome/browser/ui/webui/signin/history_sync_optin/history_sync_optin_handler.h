// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HANDLER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom-data-view.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Browser;
class Profile;

class HistorySyncOptinHandler : public history_sync_optin::mojom::PageHandler,
                                public signin::IdentityManager::Observer {
 public:
  // Initializes the handler with the mojo handlers and the needed information
  // to be displayed as well as callbacks to the main native view.
  HistorySyncOptinHandler(
      mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver,
      mojo::PendingRemote<history_sync_optin::mojom::Page> page,
      Browser* browser,
      Profile* profile,
      std::optional<bool> should_close_modal_dialog,
      HistorySyncOptinHelper::FlowCompletedCallback
          history_optin_completed_callback);
  ~HistorySyncOptinHandler() override;

  HistorySyncOptinHandler(const HistorySyncOptinHandler&) = delete;
  HistorySyncOptinHandler& operator=(const HistorySyncOptinHandler&) = delete;

  // history_sync_optin::mojom::PageHandler:
  void Accept() override;
  void Reject() override;
  void RequestAccountInfo() override;
  void UpdateDialogHeight(uint32_t height) override;

 private:
  // Gets the account info of the signed-in user if there is one, or
  // starts observing the identity manager listening for account info updates.
  void MaybeGetAccountInfo();
  // Closes the modal dialog, if one is open.
  void FinishAndCloseDialog(HistorySyncOptinHelper::ScreenChoiceResult result);
  // Adds sync consent to the history data type.
  void AddHistorySyncConsent();

  void OnScreenModeChanged(history_sync_optin::mojom::ScreenMode screen_mode);
  void OnAvatarChanged(const AccountInfo& info);
  void DispatchAccountInfoUpdate(const AccountInfo& info);

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // Called when `screen_mode_timeout_` times out.
  void OnScreenModeTimeout();

  // Allows handling received messages from the web ui page.
  const mojo::Receiver<history_sync_optin::mojom::PageHandler> receiver_;
  // Interface to send information to the web ui page.
  const mojo::Remote<history_sync_optin::mojom::Page> page_;

  const base::WeakPtr<Browser> browser_;
  const raw_ptr<Profile> profile_;
  std::optional<bool> should_close_modal_dialog_;
  HistorySyncOptinHelper::FlowCompletedCallback
      history_optin_completed_callback_;

  const raw_ptr<signin::IdentityManager> identity_manager_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  bool avatar_changed_ = false;
  bool screen_mode_changed_ = false;
  history_sync_optin::mojom::ScreenMode screen_mode_ =
      history_sync_optin::mojom::ScreenMode::kPending;
  base::OneShotTimer screen_mode_timeout_;

  // Tracks time that passes between the UI is fully initialized, but the
  // Accept / Reject buttons are not ready yet.
  std::optional<base::ElapsedTimer> user_visible_latency_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HANDLER_H_
