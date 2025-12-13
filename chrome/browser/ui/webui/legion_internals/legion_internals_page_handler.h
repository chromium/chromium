// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/legion_internals/legion_internals.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace legion {
class Client;
}

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

class LegionInternalsPageHandler
    : public legion_internals::mojom::LegionInternalsPageHandler {
 public:
  explicit LegionInternalsPageHandler(
      network::mojom::NetworkContext* network_context_,
      mojo::PendingReceiver<legion_internals::mojom::LegionInternalsPageHandler>
          receiver);
  ~LegionInternalsPageHandler() override;

  LegionInternalsPageHandler(const LegionInternalsPageHandler&) = delete;
  LegionInternalsPageHandler& operator=(const LegionInternalsPageHandler&) =
      delete;

  // legion_internals::mojom::LegionInternalsPageHandler:
  void Connect(const std::string& url,
               const std::string& api_key,
               ConnectCallback callback) override;
  void Close(CloseCallback callback) override;
  void SendRequest(const std::string& feature_name,
                   const std::string& request,
                   SendRequestCallback callback) override;

 private:
  std::unique_ptr<legion::Client> client_;
  raw_ptr<network::mojom::NetworkContext> network_context_;
  mojo::Receiver<legion_internals::mojom::LegionInternalsPageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_PAGE_HANDLER_H_
