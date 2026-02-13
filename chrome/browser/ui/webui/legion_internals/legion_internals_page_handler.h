// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/webui/legion_internals/legion_internals.mojom.h"
#include "components/legion/client.h"
#include "components/legion/common/legion_logger.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace legion {
class Client;
namespace phosphor {
class TokenManager;
}  // namespace phosphor
}  // namespace legion

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

class LegionInternalsPageHandler
    : public legion_internals::mojom::LegionInternalsPageHandler,
      public legion::LegionLogger::Observer {
 public:
  explicit LegionInternalsPageHandler(
      legion::phosphor::TokenManager* token_manager,
      network::mojom::NetworkContext* network_context,
      legion::Client* private_ai_client,
      mojo::PendingReceiver<legion_internals::mojom::LegionInternalsPageHandler>
          receiver);
  ~LegionInternalsPageHandler() override;

  LegionInternalsPageHandler(const LegionInternalsPageHandler&) = delete;
  LegionInternalsPageHandler& operator=(const LegionInternalsPageHandler&) =
      delete;

  // legion_internals::mojom::LegionInternalsPageHandler:
  void SetPage(mojo::PendingRemote<legion_internals::mojom::LegionInternalsPage>
                   page) override;
  void Connect(const std::string& url,
               const std::string& api_key,
               ConnectCallback callback) override;
  void Close(CloseCallback callback) override;
  void SendRequest(const std::string& feature_name,
                   const std::string& request,
                   SendRequestCallback callback) override;

  // legion::LegionLogger::Observer:
  void OnLogInfo(const base::Location& location,
                 std::string_view message) override;
  void OnLogError(const base::Location& location,
                  std::string_view message) override;

 private:
  void LogToPage(legion_internals::mojom::LogLevel level,
                 const base::Location& location,
                 std::string_view message);

  raw_ptr<legion::phosphor::TokenManager> token_manager_;
  // The global client, only used for observation.
  raw_ptr<legion::Client> private_ai_client_;
  // The client created by webui. Used for testing.
  std::unique_ptr<legion::Client> webui_client_;
  raw_ptr<network::mojom::NetworkContext> network_context_;
  mojo::Receiver<legion_internals::mojom::LegionInternalsPageHandler> receiver_;
  mojo::Remote<legion_internals::mojom::LegionInternalsPage> page_;

  base::ScopedMultiSourceObservation<legion::LegionLogger,
                                     legion::LegionLogger::Observer>
      scoped_logger_observations_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_PAGE_HANDLER_H_
