// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/network_conditions_override_manager.h"

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/network_conditions.h"
#include "chrome/test/chromedriver/chrome/status.h"

NetworkConditionsOverrideManager::NetworkConditionsOverrideManager(
    DevToolsClient* client)
    : client_(client), overridden_network_conditions_(nullptr) {
  client_->AddListener(this);
}

NetworkConditionsOverrideManager::~NetworkConditionsOverrideManager() {
}

Status NetworkConditionsOverrideManager::OverrideNetworkConditions(
    const NetworkConditions& network_conditions) {
  Status status = ApplyOverride(&network_conditions);
  if (status.IsOk())
    overridden_network_conditions_ = &network_conditions;
  return status;
}

Status NetworkConditionsOverrideManager::OnConnected(DevToolsClient* client) {
  return ApplyOverrideIfNeeded();
}

Status NetworkConditionsOverrideManager::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::Value::Dict& params) {
  if (method == "Page.frameNavigated") {
    if (!params.FindByDottedPath("frame.parentId"))
      return ApplyOverrideIfNeeded();
  }
  return Status(kOk);
}

Status NetworkConditionsOverrideManager::ApplyOverrideIfNeeded() {
  if (overridden_network_conditions_)
    return ApplyOverride(overridden_network_conditions_);
  return Status(kOk);
}

Status NetworkConditionsOverrideManager::ApplyOverride(
    const NetworkConditions* network_conditions) {
  base::Value::Dict params, empty_params;
  params.Set("offline", network_conditions->offline);
  params.Set("latency", network_conditions->latency);
  params.Set("downloadThroughput", network_conditions->download_throughput);
  params.Set("uploadThroughput", network_conditions->upload_throughput);

  Status status = client_->SendCommand("Network.enable", empty_params);
  if (status.IsError())
    return status;

  base::Value::Dict result;
  status = client_->SendCommandAndGetResult(
      "Network.canEmulateNetworkConditions", empty_params, &result);
  std::optional<bool> can = result.FindBool("result");
  if (status.IsError() || !can)
    return Status(kUnknownError,
        "unable to detect if chrome can emulate network conditions", status);
  if (!can.value())
    return Status(kUnknownError, "Cannot emulate network conditions");

  return client_->SendCommand("Network.emulateNetworkConditions", params);
}
