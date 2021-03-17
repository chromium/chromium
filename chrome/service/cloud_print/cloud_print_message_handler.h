// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_MESSAGE_HANDLER_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_MESSAGE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/common/cloud_print.mojom.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace cloud_print {

// Handles IPC messages for Cloud Print. Lives on the main thread.
class CloudPrintMessageHandler : public cloud_print::mojom::CloudPrint {
 public:
  explicit CloudPrintMessageHandler(CloudPrintProxy::Provider* proxy_provider);
  ~CloudPrintMessageHandler() override;

  static void Create(
      CloudPrintProxy::Provider* proxy_provider,
      mojo::PendingReceiver<cloud_print::mojom::CloudPrint> receiver);

 private:
  // cloud_print::mojom::CloudPrintProxy.
  void EnableCloudPrintProxyWithRobot(const std::string& robot_auth_code,
                                      const std::string& robot_email,
                                      const std::string& user_email,
                                      base::Value user_settings) override;
  void GetCloudPrintProxyInfo(GetCloudPrintProxyInfoCallback callback) override;
  void GetPrinters(GetPrintersCallback callback) override;
  void DisableCloudPrintProxy() override;

  CloudPrintProxy::Provider* proxy_provider_;  // Owned by our owner.

  DISALLOW_COPY_AND_ASSIGN(CloudPrintMessageHandler);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_MESSAGE_HANDLER_H_
