// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HANDLER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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
      Profile* profile);
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
  void FinishAndCloseDialog();
  // Adds sync consent to the history data type.
  void AddHistorySyncConsent();

  void OnAvatarChanged(const AccountInfo& info);
  void DispatchAccountInfoUpdate(const AccountInfo& info);

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // Allows handling received messages from the web ui page.
  const mojo::Receiver<history_sync_optin::mojom::PageHandler> receiver_;
  // Interface to send information to the web ui page.
  const mojo::Remote<history_sync_optin::mojom::Page> page_;

  const base::WeakPtr<Browser> browser_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HANDLER_H_
