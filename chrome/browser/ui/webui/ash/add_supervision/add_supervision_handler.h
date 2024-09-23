// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ADD_SUPERVISION_ADD_SUPERVISION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ADD_SUPERVISION_ADD_SUPERVISION_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision.mojom.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

class GoogleServiceAuthError;

namespace ash {

class AddSupervisionHandler
    : public add_supervision::mojom::AddSupervisionHandler,
      public signin::IdentityManager::Observer {
 public:
  // Interface for Delegates for specific behavior of AddSupervisionHandler.
  class Delegate {
   public:
    // Implementing methods should override this to implement
    // the request to close the Add Supervision dialog and return
    // a boolean to indicate whether the dialog is closing.
    virtual bool CloseDialog() = 0;

    // Allows controlling the behavior of the Add Supervision dialog when
    // the user presses Escape (if enabled, Escape closes the dialog).
    // Disabling the Escape key allows using it for actions inside the
    // webview itself (e.g., as an accessibility shortcut).
    virtual void SetCloseOnEscape(bool) = 0;
  };

  // |delegate| is owned by the caller and its lifetime must outlive |this|.
  AddSupervisionHandler(
      mojo::PendingReceiver<add_supervision::mojom::AddSupervisionHandler>
          receiver,
      content::WebUI* web_ui,
      signin::IdentityManager* identity_manager,
      Delegate* delegate);

  AddSupervisionHandler(const AddSupervisionHandler&) = delete;
  AddSupervisionHandler& operator=(const AddSupervisionHandler&) = delete;

  ~AddSupervisionHandler() override;

  // add_supervision::mojom::AddSupervisionHandler overrides:
  void RequestClose(RequestCloseCallback callback) override;
  void GetInstalledArcApps(GetInstalledArcAppsCallback callback) override;
  void GetOAuthToken(GetOAuthTokenCallback callback) override;
  void LogOut() override;
  void NotifySupervisionEnabled() override;
  void SetCloseOnEscape(bool enabled) override;

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  void OnAccessTokenFetchComplete(GetOAuthTokenCallback callback,
                                  GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);

  // The AddSupervisionUI that this AddSupervisionHandler belongs to.
  raw_ptr<content::WebUI> web_ui_;

  // Used to fetch OAuth2 access tokens.
  // The pointer to the identity manager gets reset when an identity manager
  // shutdown is detected.
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<signin::AccessTokenFetcher> oauth2_access_token_fetcher_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  mojo::Receiver<add_supervision::mojom::AddSupervisionHandler> receiver_;

  raw_ptr<Delegate> delegate_;

  base::WeakPtrFactory<AddSupervisionHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ADD_SUPERVISION_ADD_SUPERVISION_HANDLER_H_
