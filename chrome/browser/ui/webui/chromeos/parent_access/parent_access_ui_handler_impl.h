// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_UI_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_UI_HANDLER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

class GoogleServiceAuthError;

namespace chromeos {

class ParentAccessUIHandlerImpl
    : public parent_access_ui::mojom::ParentAccessUIHandler {
 public:
  ParentAccessUIHandlerImpl(
      mojo::PendingReceiver<parent_access_ui::mojom::ParentAccessUIHandler>
          receiver,
      content::WebUI* web_ui,
      signin::IdentityManager* identity_manager);
  ParentAccessUIHandlerImpl(const ParentAccessUIHandlerImpl&) = delete;
  ParentAccessUIHandlerImpl& operator=(const ParentAccessUIHandlerImpl&) =
      delete;
  ~ParentAccessUIHandlerImpl() override;

  // parent_access_ui::mojom::ParentAccessUIHandler overrides:
  void GetOAuthToken(GetOAuthTokenCallback callback) override;
  // Called when the message from the parent access server app was received.
  // encoded_parent_access_callback is a base64 encoded protocol buffer with
  // the received message. 'callback' is a mojo callback used to pass the
  // parsed message back to the WebUI.
  void OnParentAccessCallbackReceived(
      const std::string& encoded_parent_access_callback_proto,
      OnParentAccessCallbackReceivedCallback callback) override;
  void GetParentAccessParams(GetParentAccessParamsCallback callback) override;

 private:
  void OnAccessTokenFetchComplete(GetOAuthTokenCallback callback,
                                  GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);

  // Used to fetch OAuth2 access tokens.
  signin::IdentityManager* identity_manager_ = nullptr;
  std::unique_ptr<signin::AccessTokenFetcher> oauth2_access_token_fetcher_;

  mojo::Receiver<parent_access_ui::mojom::ParentAccessUIHandler> receiver_;

  base::WeakPtrFactory<ParentAccessUIHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_UI_HANDLER_IMPL_H_
