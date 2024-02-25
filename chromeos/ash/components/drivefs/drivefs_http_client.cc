// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_http_client.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace drivefs {
namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("drivefs_http_client", R"(
      semantics {
          sender: "Files App - Google Drive"
          description: "Files App integrates with Google Drive to provide a "
          "local view of what is available on the Google Drive Web interface. "
          "This allows users to navigate Google Drive as if it was a local "
          "file system."
          trigger: "User navigates through the Google Drive directory in the "
          "Files App. User opens a file in the Google Drive directory in the "
          "Files App. Additionally, the Files App will sync Google Drive data "
          "in the background as changes are made on the web or on other "
          "devices."
          data: "All metadata related to files stored in Google Drive as well "
          "as content of files stored in Google Drive."
          destination: GOOGLE_OWNED_SERVICE
          }
        policy {
          cookies_allowed: NO
          chrome_policy {
            DriveDisabled: {
                DriveDisabled: true
            }
          }
        }
        comments: "There are two policies that control this integration. "
        "DriveDisabled will disable all communications while "
        "DriveDisabledOverCellular will disable communication over cellular "
        "networks"
    )");

class DriveFsURLLoaderClient : public network::mojom::URLLoaderClient,
                               public network::mojom::DataPipeGetter {
 public:
  DriveFsURLLoaderClient(
      mojo::PendingRemote<mojom::HttpDelegate> http_delegate_remote,
      const mojom::HttpRequestPtr& request,
      mojo::PendingReceiver<network::mojom::DataPipeGetter> data_pipe_receiver,
      mojo::PendingRemote<network::mojom::URLLoader> loader_remote)
      : request_body_bytes_(request->request_body_bytes),
        loader_remote_(std::move(loader_remote)),
        http_delegate_remote_(std::move(http_delegate_remote)) {
    Clone(std::move(data_pipe_receiver));
    http_delegate_remote_.set_disconnect_handler(
        base::BindOnce(&DriveFsURLLoaderClient::OnHttpDelegateDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  enum class CallbackState : size_t {
    kBodyRequested,
    kResponseReceived,
    kRequestComplete,
    // Add new states above.
    kMin = kBodyRequested,
    kMax = kRequestComplete,
  };

  bool IsFirstCall(CallbackState state) {
    if (callback_state_.Has(state)) {
      return false;
    }
    callback_state_.Put(state);
    return true;
  }

  void OnHttpDelegateDisconnect() {
    // Cancel the request: The DriveFS side disconnected.
    loader_remote_.reset();
  }

  // URLLoaderClient Impl
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    DCHECK(IsFirstCall(CallbackState::kResponseReceived));
    std::vector<mojom::HttpHeaderPtr> headers;
    size_t iter = 0;
    std::string name;
    std::string value;
    while (response_head->headers->EnumerateHeaderLines(&iter, &name, &value)) {
      headers.push_back(mojom::HttpHeader::New(name, value));
    }
    http_delegate_remote_->OnReceiveResponse(mojom::HttpResponse::New(
        response_head->headers->response_code(), std::move(headers)));
    if (body) {
      http_delegate_remote_->OnReceiveBody(std::move(body));
    }
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    // Cancel the request: Redirects are not permitted for security reasons.
    loader_remote_.reset();
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    std::move(ack_callback).Run();
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    network::RecordOnTransferSizeUpdatedUMA(
        network::OnTransferSizeUpdatedFrom::kDriveFsURLLoaderClient);
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    DCHECK(IsFirstCall(CallbackState::kRequestComplete));
    http_delegate_remote_->OnRequestComplete(mojom::HttpCompletionStatus::New(
        static_cast<mojom::NetError>(status.error_code),
        status.decoded_body_length));
  }

  // DataPipeGetter Impl
  void Read(mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override {
    DCHECK(request_body_bytes_);
    DCHECK(IsFirstCall(CallbackState::kBodyRequested));
    std::move(callback).Run(net::OK, request_body_bytes_);
    http_delegate_remote_->GetRequestBody(std::move(pipe));
  }

  void Clone(
      mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) override {
    data_pipe_receivers_.Add(this, std::move(receiver));
  }

  const int64_t request_body_bytes_;
  base::EnumSet<CallbackState, CallbackState::kMin, CallbackState::kMax>
      callback_state_;
  mojo::ReceiverSet<network::mojom::DataPipeGetter> data_pipe_receivers_;
  mojo::Remote<network::mojom::URLLoader> loader_remote_;
  mojo::Remote<mojom::HttpDelegate> http_delegate_remote_;
  base::WeakPtrFactory<DriveFsURLLoaderClient> weak_ptr_factory_{this};
};

}  // namespace

DriveFsHttpClient::DriveFsHttpClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : throttling_profile_id_(base::UnguessableToken::Create()),
      url_loader_factory_(std::move(url_loader_factory)) {}

DriveFsHttpClient::~DriveFsHttpClient() = default;

void DriveFsHttpClient::ExecuteHttpRequest(
    mojom::HttpRequestPtr request,
    mojo::PendingRemote<mojom::HttpDelegate> delegate) {
  // Build a `URLLoaderClient` for the request. This client will bridge
  // communication between DriveFS and Chrome OS.
  mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client;
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter;
  mojo::PendingRemote<network::mojom::URLLoader> url_loader;
  mojo::PendingReceiver<network::mojom::URLLoader> url_loader_reciever =
      url_loader.InitWithNewPipeAndPassReceiver();
  mojo::ReceiverId client_id =
      clients_.Add(std::make_unique<DriveFsURLLoaderClient>(
                       std::move(delegate), request,
                       data_pipe_getter.InitWithNewPipeAndPassReceiver(),
                       std::move(url_loader)),
                   url_loader_client.InitWithNewPipeAndPassReceiver());
  // Translate the `HttpRequest` from DriveFS into a `network::ResourceRequest`.
  network::ResourceRequest resource_request;
  resource_request.url = GURL(request->url);
  resource_request.method = request->method;
  for (const auto& header : request->headers) {
    resource_request.headers.SetHeader(header->key, header->value);
  }
  // TODO(b/284789869): The Chrome network service currently automatically
  // appends a `If-None-Match` header to requests, this causes a 503 error on
  // the Drive API. For now, don't cache anything until that 503 has been fixed.
  resource_request.headers.SetHeader("Cache-Control", "no-cache");
  if (request->request_body_bytes > 0) {
    resource_request.request_body = new network::ResourceRequestBody();
    resource_request.request_body->AppendDataPipe(std::move(data_pipe_getter));
  }
  resource_request.throttling_profile_id = throttling_profile_id_;
  // Start execution, the `DriveFsURLLoaderClient` will remove itself from the
  // `clients_` map on completion.
  url_loader_factory_->CreateLoaderAndStart(
      std::move(url_loader_reciever), /*request_id=*/client_id,
      /*options=*/network::mojom::kURLLoadOptionBlockAllCookies,
      std::move(resource_request), std::move(url_loader_client),
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
}

}  // namespace drivefs
