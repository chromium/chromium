// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dappnet/dappnet_settings_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

DappnetSettingsHandler::DappnetSettingsHandler() = default;

DappnetSettingsHandler::~DappnetSettingsHandler() = default;

void DappnetSettingsHandler::RegisterMessages() {
  profile_ = Profile::FromWebUI(web_ui());
}

void DappnetSettingsHandler::OnJavascriptAllowed() {
  // Called when JavaScript is allowed
}

void DappnetSettingsHandler::OnJavascriptDisallowed() {
  // Reset the mojo receiver when JavaScript is disabled
  receiver_.reset();
}

void DappnetSettingsHandler::BindInterface(
    mojo::PendingReceiver<dappnet::mojom::DappnetSettingsHandler> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

// dappnet::mojom::DappnetSettingsHandler implementation:

void DappnetSettingsHandler::GetRpcEndpoints(GetRpcEndpointsCallback callback) {
  if (!profile_) {
    std::move(callback).Run({});
    return;
  }
  
  PrefService* prefs = profile_->GetPrefs();
  const base::Value::List& endpoints_list = 
      prefs->GetList("dappnet.rpc_endpoints");
  
  std::vector<dappnet::mojom::RpcEndpointPtr> endpoints;
  for (const auto& value : endpoints_list) {
    const base::Value::Dict* endpoint_dict = value.GetIfDict();
    if (!endpoint_dict) continue;
    
    auto endpoint = dappnet::mojom::RpcEndpoint::New();
    const std::string* id_ptr = endpoint_dict->FindString("id");
    endpoint->id = id_ptr ? *id_ptr : "";
    const std::string* url_ptr = endpoint_dict->FindString("url");
    endpoint->url = url_ptr ? *url_ptr : "";
    const std::string* name_ptr = endpoint_dict->FindString("name");
    endpoint->name = name_ptr ? *name_ptr : "";
    std::optional<int> chain_id = endpoint_dict->FindInt("chain_id");
    endpoint->chain_id = chain_id.value_or(1);
    std::optional<bool> is_default = endpoint_dict->FindBool("is_default");
    endpoint->is_default = is_default.value_or(false);
    
    endpoints.push_back(std::move(endpoint));
  }
  
  std::move(callback).Run(std::move(endpoints));
}

void DappnetSettingsHandler::AddRpcEndpoint(
    dappnet::mojom::RpcEndpointPtr endpoint,
    AddRpcEndpointCallback callback) {
  if (!profile_) {
    std::move(callback).Run(false, "Profile not available");
    return;
  }
  
  // Basic validation
  GURL url(endpoint->url);
  if (!url.is_valid() || (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsWSOrWSS())) {
    std::move(callback).Run(false, "Invalid URL");
    return;
  }
  
  if (endpoint->chain_id <= 0) {
    std::move(callback).Run(false, "Invalid chain ID");
    return;
  }
  
  PrefService* prefs = profile_->GetPrefs();
  const base::Value::List& existing_endpoints = 
      prefs->GetList("dappnet.rpc_endpoints");
  
  base::Value::List updated_endpoints = existing_endpoints.Clone();
  base::Value::Dict new_endpoint;
  new_endpoint.Set("id", endpoint->id);
  new_endpoint.Set("url", endpoint->url);
  new_endpoint.Set("name", endpoint->name);
  new_endpoint.Set("chain_id", endpoint->chain_id);
  new_endpoint.Set("is_default", endpoint->is_default);
  
  updated_endpoints.Append(std::move(new_endpoint));
  prefs->SetList("dappnet.rpc_endpoints", std::move(updated_endpoints));
  
  std::move(callback).Run(true, "");
}

void DappnetSettingsHandler::UpdateRpcEndpoint(
    const std::string& id,
    dappnet::mojom::RpcEndpointPtr endpoint,
    UpdateRpcEndpointCallback callback) {
  if (!profile_) {
    std::move(callback).Run(false);
    return;
  }
  
  PrefService* prefs = profile_->GetPrefs();
  base::Value::List endpoints = prefs->GetList("dappnet.rpc_endpoints").Clone();
  
  bool found = false;
  for (auto& value : endpoints) {
    base::Value::Dict* endpoint_dict = value.GetIfDict();
    if (endpoint_dict) {
      const std::string* endpoint_id = endpoint_dict->FindString("id");
      if (endpoint_id && *endpoint_id == id) {
        endpoint_dict->Set("url", endpoint->url);
        endpoint_dict->Set("name", endpoint->name);
        endpoint_dict->Set("chain_id", endpoint->chain_id);
        endpoint_dict->Set("is_default", endpoint->is_default);
        found = true;
        break;
      }
    }
  }
  
  if (found) {
    prefs->SetList("dappnet.rpc_endpoints", std::move(endpoints));
  }
  
  std::move(callback).Run(found);
}

void DappnetSettingsHandler::RemoveRpcEndpoint(
    const std::string& id,
    RemoveRpcEndpointCallback callback) {
  if (!profile_) {
    std::move(callback).Run(false);
    return;
  }
  
  PrefService* prefs = profile_->GetPrefs();
  base::Value::List endpoints = prefs->GetList("dappnet.rpc_endpoints").Clone();
  
  bool found = false;
  for (auto it = endpoints.begin(); it != endpoints.end(); ++it) {
    const base::Value::Dict* endpoint_dict = it->GetIfDict();
    if (endpoint_dict) {
      const std::string* endpoint_id = endpoint_dict->FindString("id");
      if (endpoint_id && *endpoint_id == id) {
        endpoints.erase(it);
        found = true;
        break;
      }
    }
  }
  
  if (found) {
    prefs->SetList("dappnet.rpc_endpoints", std::move(endpoints));
  }
  
  std::move(callback).Run(found);
}

void DappnetSettingsHandler::TestRpcConnection(
    const std::string& url,
    TestRpcConnectionCallback callback) {
  // Just validate URL format
  GURL gurl(url);
  if (!gurl.is_valid() || (!gurl.SchemeIsHTTPOrHTTPS() && !gurl.SchemeIsWSOrWSS())) {
    std::move(callback).Run(false, "Invalid URL format");
    return;
  }
  
  // For demo, always return success for valid URLs
  std::move(callback).Run(true, "Connection test successful (demo)");
}

void DappnetSettingsHandler::SetDefaultRpc(
    const std::string& id,
    SetDefaultRpcCallback callback) {
  if (!profile_) {
    std::move(callback).Run(false);
    return;
  }
  
  PrefService* prefs = profile_->GetPrefs();
  base::Value::List endpoints = prefs->GetList("dappnet.rpc_endpoints").Clone();
  
  bool found = false;
  for (auto& value : endpoints) {
    base::Value::Dict* endpoint_dict = value.GetIfDict();
    if (endpoint_dict) {
      const std::string* endpoint_id = endpoint_dict->FindString("id");
      if (endpoint_id && *endpoint_id == id) {
        endpoint_dict->Set("is_default", true);
        found = true;
      } else {
        endpoint_dict->Set("is_default", false);
      }
    }
  }
  
  if (found) {
    prefs->SetList("dappnet.rpc_endpoints", std::move(endpoints));
  }
  
  std::move(callback).Run(found);
}

// Service status methods - all return mock data for UI demo
void DappnetSettingsHandler::GetGatewayStatus(GetGatewayStatusCallback callback) {
  auto status = dappnet::mojom::GatewayStatus::New();
  status->is_running = false;
  status->port = 8080;
  status->pid = 0;
  status->error_message = "Service management disabled (UI demo)";
  std::move(callback).Run(std::move(status));
}

void DappnetSettingsHandler::StartGateway(StartGatewayCallback callback) {
  std::move(callback).Run(false, "Service management disabled (UI demo)");
}

void DappnetSettingsHandler::StopGateway(StopGatewayCallback callback) {
  std::move(callback).Run(true);
}

void DappnetSettingsHandler::RestartGateway(RestartGatewayCallback callback) {
  std::move(callback).Run(false, "Service management disabled (UI demo)");
}

void DappnetSettingsHandler::GetIpfsStatus(GetIpfsStatusCallback callback) {
  auto status = dappnet::mojom::IpfsStatus::New();
  status->is_running = false;
  status->api_port = 5001;
  status->gateway_port = 8081;
  status->peer_count = 0;
  std::move(callback).Run(std::move(status));
}

void DappnetSettingsHandler::StartIpfs(StartIpfsCallback callback) {
  std::move(callback).Run(false, "Service management disabled (UI demo)");
}

void DappnetSettingsHandler::StopIpfs(StopIpfsCallback callback) {
  std::move(callback).Run(true);
}

void DappnetSettingsHandler::RestartIpfs(RestartIpfsCallback callback) {
  std::move(callback).Run(false, "Service management disabled (UI demo)");
}