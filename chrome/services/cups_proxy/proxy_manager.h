// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PROXY_MANAGER_H_
#define CHROME_SERVICES_CUPS_PROXY_PROXY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/services/cups_proxy/public/mojom/proxy.mojom.h"
#include "chrome/services/ipp_parser/public/cpp/ipp_converter.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace cups_proxy {

class CupsProxyServiceDelegate;
class IppValidator;
class PrinterInstaller;
class SocketManager;

// mojom::CupsProxier handler.
//
// This handler's job is vetting incoming arbitrary CUPS IPP requests before
// they reach the CUPS Daemon. Requests are parsed out-of-process, by the
// CupsIppParser Service, and validated/rebuilt in-process before being proxied.
// This handler must be created/accessed from a seqeunced context.
//
// Note: This handler only supports processing one request at a time; any
// concurrent requests will immediately fail with an empty response.
class ProxyManager : public mojom::CupsProxier {
 public:
  // Factory function.
  static std::unique_ptr<ProxyManager> Create(
      mojo::PendingReceiver<mojom::CupsProxier> request,
      std::unique_ptr<CupsProxyServiceDelegate> delegate);

  // Factory function that allows injected dependencies, for testing.
  static std::unique_ptr<ProxyManager> CreateForTesting(
      mojo::PendingReceiver<mojom::CupsProxier> request,
      std::unique_ptr<CupsProxyServiceDelegate> delegate,
      std::unique_ptr<IppValidator> ipp_validator,
      std::unique_ptr<PrinterInstaller> printer_installer,
      std::unique_ptr<SocketManager> socket_manager);

  ~ProxyManager() override = default;

  void ProxyRequest(const std::string& method,
                    const std::string& url,
                    const std::string& version,
                    const std::vector<ipp_converter::HttpHeader>& headers,
                    const std::vector<uint8_t>& body,
                    ProxyRequestCallback cb) override = 0;
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_PROXY_MANAGER_H_
