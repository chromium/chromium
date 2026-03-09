// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals.mojom.h"
#include "components/private_ai/client.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace private_ai {

class Client;

namespace phosphor {
class TokenManager;
}  // namespace phosphor

class PrivateAiInternalsPageHandler
    : public private_ai_internals::mojom::PrivateAiInternalsPageHandler,
      public PrivateAiLogger::Observer {
 public:
  explicit PrivateAiInternalsPageHandler(
      phosphor::TokenManager* token_manager,
      network::mojom::NetworkContext* network_context,
      Client* private_ai_client,
      mojo::PendingReceiver<
          private_ai_internals::mojom::PrivateAiInternalsPageHandler> receiver);
  ~PrivateAiInternalsPageHandler() override;

  PrivateAiInternalsPageHandler(const PrivateAiInternalsPageHandler&) = delete;
  PrivateAiInternalsPageHandler& operator=(
      const PrivateAiInternalsPageHandler&) = delete;

  // private_ai_internals::mojom::PrivateAiInternalsPageHandler:
  void SetPage(
      mojo::PendingRemote<private_ai_internals::mojom::PrivateAiInternalsPage>
          page) override;
  void Connect(const std::string& url,
               const std::string& api_key,
               const std::string& proxy_url,
               bool use_token_attestation,
               ConnectCallback callback) override;
  void Close(CloseCallback callback) override;
  void SendRequest(const std::string& feature_name,
                   const std::string& request,
                   SendRequestCallback callback) override;

  // PrivateAiLogger::Observer:
  void OnLogInfo(const base::Location& location,
                 std::string_view message) override;
  void OnLogError(const base::Location& location,
                  std::string_view message) override;

  static constexpr char kApiKeyPlaceholder[] = "__DEFAULT_API_KEY__";

 private:
  void LogToPage(private_ai_internals::mojom::LogLevel level,
                 const base::Location& location,
                 std::string_view message);

  raw_ptr<phosphor::TokenManager> token_manager_;
  // The global client, only used for observation.
  raw_ptr<Client> private_ai_client_;
  // The client created by webui. Used for testing.
  std::unique_ptr<PrivateAiLogger> webui_logger_;
  std::unique_ptr<Client> webui_client_;
  raw_ptr<network::mojom::NetworkContext> network_context_;
  mojo::Receiver<private_ai_internals::mojom::PrivateAiInternalsPageHandler>
      receiver_;
  mojo::Remote<private_ai_internals::mojom::PrivateAiInternalsPage> page_;

  base::ScopedMultiSourceObservation<PrivateAiLogger, PrivateAiLogger::Observer>
      scoped_logger_observations_{this};
};

}  // namespace private_ai

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_PAGE_HANDLER_H_
