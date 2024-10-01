// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_constants.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-shared.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace ash::babelorca {

TachyonStreamingClient::TachyonStreamingClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ParsingServiceBinder binder_callback,
    OnMessageCallback on_message_callback)
    : url_loader_factory_(url_loader_factory),
      binder_callback_(std::move(binder_callback)),
      on_message_callback_(std::move(on_message_callback)) {}

TachyonStreamingClient::~TachyonStreamingClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TachyonStreamingClient::StartRequest(
    std::unique_ptr<RequestDataWrapper> request_data,
    std::string oauth_token,
    AuthFailureCallback auth_failure_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_data_ = std::move(request_data);
  auth_failure_cb_ = std::move(auth_failure_cb);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(request_data_->url);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.AddHeaderFromString(
      base::StringPrintf(kOauthHeaderTemplate, oauth_token.c_str()));

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 request_data_->annotation_tag);
  if (request_data_->max_retries > 0) {
    const int retry_mode = network::SimpleURLLoader::RETRY_ON_5XX |
                           network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE;
    url_loader_->SetRetryOptions(request_data_->max_retries, retry_mode);
  }
  url_loader_->AttachStringForUpload(request_data_->content_data,
                                     "application/x-protobuf");
  url_loader_->DownloadAsStream(url_loader_factory_.get(), this);
}

void TachyonStreamingClient::OnDataReceived(std::string_view string_piece,
                                            base::OnceClosure resume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!parsing_service_.is_bound()) {
    parsing_service_ = binder_callback_.Run();
    parsing_service_.set_disconnect_handler(
        base::BindOnce(&TachyonStreamingClient::OnParsingServiceDisconnected,
                       base::Unretained(this)));
  }
  parsing_service_->Parse(
      std::string(string_piece),
      base::BindOnce(&TachyonStreamingClient::OnParsed, base::Unretained(this),
                     std::move(resume)));
}

void TachyonStreamingClient::OnComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parsing_service_.reset();
  if (success) {
    std::move(request_data_->response_cb)
        .Run(TachyonResponse(TachyonResponse::Status::kOk));
    return;
  }
  HandleResponse(std::move(url_loader_), std::move(request_data_),
                 std::move(auth_failure_cb_), /*response_body=*/nullptr);
}

void TachyonStreamingClient::OnRetry(base::OnceClosure start_retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parsing_service_.reset();
  std::move(start_retry).Run();
}

void TachyonStreamingClient::OnParsed(
    base::OnceClosure resume,
    mojom::ParsingState parsing_state,
    std::vector<mojom::BabelOrcaMessagePtr> messages,
    mojom::StreamStatusPtr stream_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& message : messages) {
    on_message_callback_.Run(std::move(message));
  }
  if (parsing_state == mojom::ParsingState::kOk) {
    std::move(resume).Run();
    return;
  }
  url_loader_.reset();
  parsing_service_.reset();
  // Report internal error if there is a parsing error or the stream is closed
  // and stream_status is not present.
  if (parsing_state == mojom::ParsingState::kError || stream_status.is_null()) {
    std::move(request_data_->response_cb)
        .Run(TachyonResponse(TachyonResponse::Status::kInternalError));
    return;
  }
  TachyonResponse response(stream_status->code, stream_status->message);
  if (response.status() == TachyonResponse::Status::kAuthError) {
    std::move(auth_failure_cb_).Run(std::move(request_data_));
    return;
  }
  std::move(request_data_->response_cb)
      .Run(TachyonResponse(stream_status->code, stream_status->message));
}

void TachyonStreamingClient::OnParsingServiceDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_.reset();
  parsing_service_.reset();
  std::move(request_data_->response_cb)
      .Run(TachyonResponse(TachyonResponse::Status::kInternalError));
}

}  // namespace ash::babelorca
