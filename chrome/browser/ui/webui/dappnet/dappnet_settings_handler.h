// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DAPPNET_DAPPNET_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DAPPNET_DAPPNET_SETTINGS_HANDLER_H_

#include "chrome/browser/dappnet/mojom/dappnet_settings.mojom.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#include <string>

#include "base/memory/raw_ptr.h"

class Profile;

// Handles messages from the Dappnet settings page using Mojo.
class DappnetSettingsHandler : public content::WebUIMessageHandler,
                               public dappnet::mojom::DappnetSettingsHandler {
 public:
  DappnetSettingsHandler();
  ~DappnetSettingsHandler() override;

  DappnetSettingsHandler(const DappnetSettingsHandler&) = delete;
  DappnetSettingsHandler& operator=(const DappnetSettingsHandler&) = delete;

  // WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Bind the Mojo interface
  void BindInterface(
      mojo::PendingReceiver<dappnet::mojom::DappnetSettingsHandler> receiver);

 private:
  // dappnet::mojom::DappnetSettingsHandler:
  void GetRpcEndpoints(GetRpcEndpointsCallback callback) override;
  void AddRpcEndpoint(dappnet::mojom::RpcEndpointPtr endpoint,
                      AddRpcEndpointCallback callback) override;
  void UpdateRpcEndpoint(const std::string& id,
                         dappnet::mojom::RpcEndpointPtr endpoint,
                         UpdateRpcEndpointCallback callback) override;
  void RemoveRpcEndpoint(const std::string& id,
                         RemoveRpcEndpointCallback callback) override;
  void TestRpcConnection(const std::string& url,
                         TestRpcConnectionCallback callback) override;
  void SetDefaultRpc(const std::string& id,
                     SetDefaultRpcCallback callback) override;
  void GetGatewayStatus(GetGatewayStatusCallback callback) override;
  void StartGateway(StartGatewayCallback callback) override;
  void StopGateway(StopGatewayCallback callback) override;
  void RestartGateway(RestartGatewayCallback callback) override;
  void GetIpfsStatus(GetIpfsStatusCallback callback) override;
  void StartIpfs(StartIpfsCallback callback) override;
  void StopIpfs(StopIpfsCallback callback) override;
  void RestartIpfs(RestartIpfsCallback callback) override;

  raw_ptr<Profile> profile_ = nullptr;
  mojo::Receiver<dappnet::mojom::DappnetSettingsHandler> receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DAPPNET_DAPPNET_SETTINGS_HANDLER_H_