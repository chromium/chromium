// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/response_callback_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client.h"
#include "chromeos/ash/components/boca/babelorca/token_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ash::babelorca {
namespace {

std::optional<std::string> SerializeProtoToString(
    std::unique_ptr<google::protobuf::MessageLite> request_proto) {
  std::string proto_string;
  if (!request_proto->SerializeToString(&proto_string)) {
    return std::nullopt;
  }
  return proto_string;
}

}  // namespace

TachyonAuthedClientImpl::TachyonAuthedClientImpl(
    std::unique_ptr<TachyonClient> client,
    TokenManager* oauth_token_manager)
    : client_(std::move(client)), oauth_token_manager_(oauth_token_manager) {}

TachyonAuthedClientImpl::~TachyonAuthedClientImpl() = default;

void TachyonAuthedClientImpl::StartAuthedRequest(
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    std::unique_ptr<google::protobuf::MessageLite> request_proto,
    std::string_view url,
    int max_retries,
    std::unique_ptr<ResponseCallbackWrapper> response_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto serialize_cb =
      base::BindOnce(SerializeProtoToString, std::move(request_proto));
  auto reply_post_cb =
      base::BindOnce(&TachyonAuthedClientImpl::OnRequestProtoSerialized,
                     weak_ptr_factory_.GetWeakPtr(), annotation_tag, url,
                     max_retries, std::move(response_cb));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, std::move(serialize_cb), std::move(reply_post_cb));
}

void TachyonAuthedClientImpl::StartAuthedRequestString(
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    std::string request_string,
    std::string_view url,
    int max_retries,
    std::unique_ptr<ResponseCallbackWrapper> response_cb) {
  OnRequestProtoSerialized(annotation_tag, url, max_retries,
                           std::move(response_cb), request_string);
}

void TachyonAuthedClientImpl::OnRequestProtoSerialized(
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    std::string_view url,
    int max_retries,
    std::unique_ptr<ResponseCallbackWrapper> response_cb,
    std::optional<std::string> request_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!request_string) {
    response_cb->Run(base::unexpected(
        ResponseCallbackWrapper::TachyonRequestError::kInternalError));
    return;
  }
  std::unique_ptr<RequestDataWrapper> request_data =
      std::make_unique<RequestDataWrapper>(annotation_tag, url, max_retries,
                                           std::move(response_cb));
  request_data->content_data = std::move(*request_string);
  const std::string* oauth_token = oauth_token_manager_->GetTokenString();
  if (oauth_token) {
    StartAuthedRequestInternal(std::move(request_data),
                               /*has_oauth_token=*/true);
    return;
  }
  oauth_token_manager_->ForceFetchToken(
      base::BindOnce(&TachyonAuthedClientImpl::StartAuthedRequestInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request_data)));
}

void TachyonAuthedClientImpl::StartAuthedRequestInternal(
    std::unique_ptr<RequestDataWrapper> request_data,
    bool has_oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!has_oauth_token) {
    request_data->response_cb->Run(base::unexpected(
        ResponseCallbackWrapper::TachyonRequestError::kAuthError));
    return;
  }
  std::string oauth_token = *(oauth_token_manager_->GetTokenString());
  request_data->oauth_version = oauth_token_manager_->GetFetchedVersion();
  client_->StartRequest(
      std::move(request_data), std::move(oauth_token),
      base::BindOnce(&TachyonAuthedClientImpl::OnRequestAuthFailure,
                     base::Unretained(this)));
}

void TachyonAuthedClientImpl::OnRequestAuthFailure(
    std::unique_ptr<RequestDataWrapper> request_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static int constexpr kMaxAuthRetries = 1;
  if (request_data->oauth_retry_num >= kMaxAuthRetries) {
    request_data->response_cb->Run(base::unexpected(
        ResponseCallbackWrapper::TachyonRequestError::kAuthError));
    return;
  }
  ++(request_data->oauth_retry_num);
  const std::string* oauth_token = oauth_token_manager_->GetTokenString();
  // Check for the token version to make sure it is not the same as the
  // one used one in the auth failure request.
  if (oauth_token && request_data->oauth_version !=
                         oauth_token_manager_->GetFetchedVersion()) {
    StartAuthedRequestInternal(std::move(request_data),
                               /*has_oauth_token=*/true);
    return;
  }
  oauth_token_manager_->ForceFetchToken(
      base::BindOnce(&TachyonAuthedClientImpl::StartAuthedRequestInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request_data)));
}

}  // namespace ash::babelorca
