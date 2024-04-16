// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_hints/browser/simple_network_hints_handler_impl.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace network_hints {
namespace {

// Preconnects can be received from the renderer before commit messages, so
// need to use the key from the pending navigation, and not the committed
// navigation, unlike other consumers. This does mean on navigating away from a
// site, preconnect is more likely to incorrectly use the
// NetworkAnonymizationKey of the previous commit.
net::NetworkAnonymizationKey GetPendingNetworkAnonymizationKey(
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetPendingIsolationInfoForSubresources()
      .network_anonymization_key();
}

// This class contains a std::unique_ptr of itself, it is passed in through
// Start() method, and will be freed by the OnComplete() method when resolving
// has completed or mojo connection error has happened.
class DnsLookupRequest : public network::ResolveHostClientBase {
 public:
  DnsLookupRequest(int render_process_id,
                   int render_frame_id,
                   const url::SchemeHostPort& url)
      : render_process_id_(render_process_id),
        render_frame_id_(render_frame_id),
        url_(url) {}

  DnsLookupRequest(const DnsLookupRequest&) = delete;
  DnsLookupRequest& operator=(const DnsLookupRequest&) = delete;

  // Return underlying network resolver status.
  // net::OK ==> Host was found synchronously.
  // net:ERR_IO_PENDING ==> Network will callback later with result.
  // anything else ==> Host was not found synchronously.
  void Start(std::unique_ptr<DnsLookupRequest> request) {
    request_ = std::move(request);

    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
    if (!render_frame_host) {
      OnComplete(net::ERR_NAME_NOT_RESOLVED,
                 net::ResolveErrorInfo(net::ERR_FAILED),
                 /*resolved_addresses=*/std::nullopt,
                 /*endpoint_results_with_metadata=*/std::nullopt);
      return;
    }

    DCHECK(!receiver_.is_bound());
    network::mojom::ResolveHostParametersPtr resolve_host_parameters =
        network::mojom::ResolveHostParameters::New();
    // Lets the host resolver know it can be de-prioritized.
    resolve_host_parameters->initial_priority = net::RequestPriority::IDLE;
    // Make a note that this is a speculative resolve request. This allows
    // separating it from real navigations in the observer's callback.
    resolve_host_parameters->is_speculative = true;
    // TODO(crbug.com/40641818): Pass in a non-empty
    // NetworkAnonymizationKey.
    // TODO(crbug.com/40235854): Consider passing a SchemeHostPort to trigger
    // HTTPS DNS resource record query.
    render_frame_host->GetProcess()
        ->GetStoragePartition()
        ->GetNetworkContext()
        ->ResolveHost(network::mojom::HostResolverHost::NewSchemeHostPort(url_),
                      GetPendingNetworkAnonymizationKey(render_frame_host),
                      std::move(resolve_host_parameters),
                      receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(base::BindOnce(
        &DnsLookupRequest::OnComplete, base::Unretained(this),
        net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
        /*resolved_addresses=*/std::nullopt,
        /*endpoint_results_with_metadata=*/std::nullopt));
  }

 private:
  // network::mojom::ResolveHostClient:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    VLOG(2) << __FUNCTION__ << ": " << url_.Serialize()
            << ", result=" << resolve_error_info.error;
    request_.reset();
  }

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  const int render_process_id_;
  const int render_frame_id_;
  const url::SchemeHostPort url_;
  std::unique_ptr<DnsLookupRequest> request_;
};

}  // namespace

SimpleNetworkHintsHandlerImpl::SimpleNetworkHintsHandlerImpl(
    int render_process_id,
    int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}

SimpleNetworkHintsHandlerImpl::~SimpleNetworkHintsHandlerImpl() = default;

// static
void SimpleNetworkHintsHandlerImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<mojom::NetworkHintsHandler> receiver) {
  int render_process_id = frame_host->GetProcess()->GetID();
  int render_frame_id = frame_host->GetRoutingID();
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new SimpleNetworkHintsHandlerImpl(render_process_id,
                                                         render_frame_id)),
      std::move(receiver));
}

void SimpleNetworkHintsHandlerImpl::PrefetchDNS(
    const std::vector<url::SchemeHostPort>& urls) {
  for (const url::SchemeHostPort& url : urls) {
    std::unique_ptr<DnsLookupRequest> request =
        std::make_unique<DnsLookupRequest>(render_process_id_, render_frame_id_,
                                           url);
    DnsLookupRequest* request_ptr = request.get();
    request_ptr->Start(std::move(request));
  }
}

void SimpleNetworkHintsHandlerImpl::Preconnect(const url::SchemeHostPort& url,
                                               bool allow_credentials) {
  if (url.scheme() != url::kHttpScheme && url.scheme() != url::kHttpsScheme) {
    return;
  }

  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  net::NetworkAnonymizationKey network_anonymization_key =
      render_frame_host->GetPendingIsolationInfoForSubresources()
          .network_anonymization_key();

  render_frame_host->GetStoragePartition()
      ->GetNetworkContext()
      ->PreconnectSockets(/*num_streams=*/1, url.GetURL(),
                          allow_credentials
                              ? network::mojom::CredentialsMode::kInclude
                              : network::mojom::CredentialsMode::kOmit,
                          network_anonymization_key);
}

}  // namespace network_hints
