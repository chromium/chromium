// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_url_loader_factory_interceptor.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "components/guest_view/browser/slim_web_view/request_utils.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace guest_view {

namespace {

// A proxy `TrustedHeaderClient` that forwards calls to the downstream client
// and adds additional headers to the request if needed. If the guest is null,
// this class acts as a no-op.
class SlimWebViewHeaderClient : public network::mojom::TrustedHeaderClient {
 public:
  SlimWebViewHeaderClient(
      SlimWebViewGuest* guest,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_client)
      : guest_(guest ? guest->GetWeakPtr() : nullptr),
        target_client_(std::move(target_client)) {}
  ~SlimWebViewHeaderClient() override = default;

  // network::mojom::TrustedHeaderClient:
  void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                           OnBeforeSendHeadersCallback callback) override {
    net::HttpRequestHeaders modified_headers = headers;
    if (guest_) {
      const auto& params = guest_->before_send_headers_params();
      // No need to check if the request type matches, as this is already done
      // in the `URLLoaderFactoryProxy`.
      if (params.has_value()) {
        modified_headers.MergeFrom(params->add_headers);
      }
    }
    if (target_client_) {
      target_client_->OnBeforeSendHeaders(modified_headers,
                                          std::move(callback));
    } else {
      std::move(callback).Run(net::OK, modified_headers);
    }
  }

  // This is always a no-op as there is no support for modifying the response
  // headers.
  void OnHeadersReceived(const std::string& headers,
                         const net::IPEndPoint& remote_endpoint,
                         const std::optional<net::SSLInfo>& ssl_info,
                         OnHeadersReceivedCallback callback) override {
    if (target_client_) {
      target_client_->OnHeadersReceived(headers, remote_endpoint, ssl_info,
                                        std::move(callback));
    } else {
      std::move(callback).Run(net::OK, std::nullopt, std::nullopt);
    }
  }

 private:
  base::WeakPtr<SlimWebViewGuest> guest_;
  mojo::Remote<network::mojom::TrustedHeaderClient> target_client_;
};

// A proxy `URLLoaderFactory` that sets up a TrustedURLClient for requests
// that match the `before_send_headers_params` of the SlimWebViewGuest.
class URLLoaderFactoryProxy
    : public network::SelfDeletingURLLoaderFactory,
      public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  URLLoaderFactoryProxy(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
          header_client_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          target_header_client_remote,
      SlimWebViewGuest* guest,
      bool is_subframe_request)
      : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver)),
        guest_(guest->GetWeakPtr()),
        is_subframe_request_(is_subframe_request) {
    target_factory_.Bind(std::move(target_factory));
    target_factory_.set_disconnect_handler(
        base::BindOnce(&URLLoaderFactoryProxy::DisconnectReceiversAndDestroy,
                       base::Unretained(this)));
    if (header_client_receiver) {
      url_loader_header_client_receiver_.Bind(
          std::move(header_client_receiver));
    }
    if (target_header_client_remote) {
      target_url_loader_header_client_.Bind(
          std::move(target_header_client_remote));

      target_url_loader_header_client_.set_disconnect_handler(
          base::BindOnce(&URLLoaderFactoryProxy::OnTargetHeaderClientDisconnect,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
  ~URLLoaderFactoryProxy() override = default;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    if (guest_) {
      const GURL& url = request.url;
      if (auto result = guest_->IsUrlAllowed(url); !result.has_value()) {
        DVLOG(2) << "Blocked SlimWebView request: " << result.error();
        mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
            ->OnComplete(
                network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_CLIENT));
        return;
      }
    }

    auto type = RequestResourceTypeFromResourceRequest(request);
    bool should_add_headers = false;
    if (guest_) {
      const auto& params = guest_->before_send_headers_params();
      if (params.has_value() && params->resource_types.contains(type) &&
          (params->include_sub_frame_requests || !is_subframe_request_)) {
        should_add_headers = true;
      }
    }

    if (should_add_headers) {
      // Mark the request to be intercepted by the
      // `TrustedURLLoaderHeaderClient`
      options |= network::mojom::kURLLoadOptionUseHeaderClient;
      requests_with_header_params_.insert(request_id);
    }

    target_factory_->CreateLoaderAndStart(std::move(loader), request_id,
                                          options, request, std::move(client),
                                          traffic_annotation);
  }

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {
    mojo::PendingRemote<network::mojom::TrustedHeaderClient>
        target_header_client;
    if (target_url_loader_header_client_) {
      target_url_loader_header_client_->OnLoaderCreated(
          request_id, target_header_client.InitWithNewPipeAndPassReceiver());
    }

    SlimWebViewGuest* guest = nullptr;
    auto it = requests_with_header_params_.find(request_id);
    if (guest_ && it != requests_with_header_params_.end()) {
      guest = guest_.get();
      requests_with_header_params_.erase(it);
    }

    // Always create a `SlimWebViewHeaderClient`, even if the guest is null, as
    // the receiver must be bound to a valid object for the request to complete.
    mojo::MakeSelfOwnedReceiver(std::make_unique<SlimWebViewHeaderClient>(
                                    guest, std::move(target_header_client)),
                                std::move(receiver));
  }

  // This is a no-op, as there is no support for modifying the request headers
  // for CORS preflight requests.
  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {
    mojo::PendingRemote<network::mojom::TrustedHeaderClient>
        target_header_client;
    if (target_url_loader_header_client_) {
      target_url_loader_header_client_->OnLoaderForCorsPreflightCreated(
          request, target_header_client.InitWithNewPipeAndPassReceiver());
    } else {
      mojo::MakeSelfOwnedReceiver(std::make_unique<SlimWebViewHeaderClient>(
                                      nullptr, std::move(target_header_client)),
                                  std::move(receiver));
    }
  }

 private:
  using SelfDeletingURLLoaderFactory::DisconnectReceiversAndDestroy;

  void OnTargetHeaderClientDisconnect() {
    // The downstream header client factory has disconnected.
    // The remote is reset to prevent further calls. This proxy is not
    // destroyed here, as its lifecycle is tied to the main URLLoaderFactory
    // pipe (`target_factory_`).
    target_url_loader_header_client_.reset();
  }

  base::WeakPtr<SlimWebViewGuest> guest_;
  bool is_subframe_request_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient>
      target_url_loader_header_client_;
  mojo::Receiver<network::mojom::TrustedURLLoaderHeaderClient>
      url_loader_header_client_receiver_{this};

  absl::flat_hash_set<int32_t> requests_with_header_params_;

  base::WeakPtrFactory<URLLoaderFactoryProxy> weak_ptr_factory_{this};
};

}  // namespace

void MaybeInterceptURLLoaderFactoryForSlimWebView(
    content::RenderFrameHost* render_frame_host,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client) {
  if (!render_frame_host) {
    return;
  }
  auto* guest = SlimWebViewGuest::FromRenderFrameHost(render_frame_host);
  if (!guest) {
    return;
  }

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      target_header_client;
  mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
      header_client_receiver;

  // Intercept the TrustedURLLoaderHeaderClient channel to act as a proxy.
  // The original `header_client` remote, which points to the downstream
  // client, is moved to become the proxy's target. The caller's `header_client`
  // is then replaced with a new remote that points to this proxy's receiver,
  // effectively inserting the interceptor into the chain.
  if (header_client) {
    target_header_client = std::move(*header_client);
    *header_client = header_client_receiver.InitWithNewPipeAndPassRemote();
  }

  // Insert the proxy factory.
  auto [receiver, remote] = factory_builder.Append();
  // The proxy factory manages its own lifetime.
  bool is_subframe_request = render_frame_host->GetParent() != nullptr;
  new URLLoaderFactoryProxy(
      std::move(receiver), std::move(remote), std::move(header_client_receiver),
      std::move(target_header_client), guest, is_subframe_request);
}

}  // namespace guest_view
