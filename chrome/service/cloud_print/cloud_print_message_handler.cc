// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_message_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/common/cloud_print/cloud_print_proxy_info.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace cloud_print {

CloudPrintMessageHandler::CloudPrintMessageHandler(
    CloudPrintProxy::Provider* proxy_provider)
    : proxy_provider_(proxy_provider) {
  DCHECK(proxy_provider);
}

CloudPrintMessageHandler::~CloudPrintMessageHandler() = default;

// static
void CloudPrintMessageHandler::Create(
    CloudPrintProxy::Provider* proxy_provider,
    mojo::PendingReceiver<cloud_print::mojom::CloudPrint> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CloudPrintMessageHandler>(proxy_provider),
      std::move(receiver));
}

void CloudPrintMessageHandler::EnableCloudPrintProxyWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email,
    base::Value user_settings) {
  proxy_provider_->GetCloudPrintProxy()->EnableForUserWithRobot(
      robot_auth_code, robot_email, user_email, std::move(user_settings));
}

void CloudPrintMessageHandler::GetCloudPrintProxyInfo(
    GetCloudPrintProxyInfoCallback callback) {
  CloudPrintProxyInfo info;
  proxy_provider_->GetCloudPrintProxy()->GetProxyInfo(&info);
  std::move(callback).Run(info.enabled, info.email, info.proxy_id);
}

void CloudPrintMessageHandler::GetPrinters(GetPrintersCallback callback) {
  std::vector<std::string> printers;
  proxy_provider_->GetCloudPrintProxy()->GetPrinters(&printers);
  std::move(callback).Run(printers);
}

void CloudPrintMessageHandler::DisableCloudPrintProxy() {
  proxy_provider_->GetCloudPrintProxy()->UnregisterPrintersAndDisableForUser();
}

}  // namespace cloud_print
