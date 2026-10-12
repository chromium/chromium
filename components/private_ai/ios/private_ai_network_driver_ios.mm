// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/ios/private_ai_network_driver_ios.h"

#include "base/functional/bind.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/network_context.h"
#include "services/network/network_service_proxy_delegate.h"

namespace private_ai {

namespace {

// A self-owned wrapper around network::NetworkContext and its underlying
// net::URLRequestContext. It deletes itself when the Mojo connection is
// disconnected.
class SelfOwnedNetworkContext {
 public:
  SelfOwnedNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver,
      std::unique_ptr<net::URLRequestContext> url_request_context)
      : url_request_context_(std::move(url_request_context)),
        network_context_(
            /*network_service=*/nullptr,
            mojo::PendingReceiver<network::mojom::NetworkContext>(),
            url_request_context_.get(),
            /*cors_exempt_header_list=*/{}),
        receiver_(&network_context_, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &SelfOwnedNetworkContext::OnDisconnect, base::Unretained(this)));
  }

  ~SelfOwnedNetworkContext() = default;

  SelfOwnedNetworkContext(const SelfOwnedNetworkContext&) = delete;
  SelfOwnedNetworkContext& operator=(const SelfOwnedNetworkContext&) = delete;

 private:
  void OnDisconnect() { delete this; }

  std::unique_ptr<net::URLRequestContext> url_request_context_;
  network::NetworkContext network_context_;
  mojo::Receiver<network::mojom::NetworkContext> receiver_;
};

void CreateIsolatedNetworkContextOnIOThread(
    mojo::PendingReceiver<network::mojom::NetworkContext> receiver,
    network::mojom::NetworkContextParamsPtr params) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  net::URLRequestContextBuilder builder;

  if (params->initial_custom_proxy_config) {
    builder.set_proxy_delegate(
        std::make_unique<network::NetworkServiceProxyDelegate>(
            std::move(params->initial_custom_proxy_config),
            /*config_client_receiver=*/mojo::NullReceiver(),
            /*observer_remote=*/mojo::NullRemote()));
  }

  if (!params->user_agent.empty()) {
    builder.set_user_agent(params->user_agent);
  }

  auto url_request_context = builder.Build();

  new SelfOwnedNetworkContext(std::move(receiver),
                              std::move(url_request_context));
}

}  // namespace

PrivateAiNetworkDriverIOS::PrivateAiNetworkDriverIOS() = default;

PrivateAiNetworkDriverIOS::~PrivateAiNetworkDriverIOS() = default;

network::mojom::CertVerifierServiceRemoteParamsPtr
PrivateAiNetworkDriverIOS::GetCertVerifierParams() {
  return network::mojom::CertVerifierServiceRemoteParams::New();
}

void PrivateAiNetworkDriverIOS::CreateNetworkContext(
    mojo::PendingReceiver<network::mojom::NetworkContext> receiver,
    network::mojom::NetworkContextParamsPtr params) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  CHECK(receiver.is_valid());

  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CreateIsolatedNetworkContextOnIOThread,
                                std::move(receiver), std::move(params)));
}

std::optional<bool> PrivateAiNetworkDriverIOS::IsProxyConfigured() const {
  return std::nullopt;
}

}  // namespace private_ai
