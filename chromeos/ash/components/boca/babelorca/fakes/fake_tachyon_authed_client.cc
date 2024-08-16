// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/response_callback_wrapper.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ash::babelorca {

FakeTachyonAuthedClient::FakeTachyonAuthedClient() = default;

FakeTachyonAuthedClient::~FakeTachyonAuthedClient() = default;

void FakeTachyonAuthedClient::StartAuthedRequest(
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    std::unique_ptr<google::protobuf::MessageLite> request_proto,
    std::string_view url,
    int max_retries,
    std::unique_ptr<ResponseCallbackWrapper> response_cb) {
  StartAuthedRequestString(annotation_tag, request_proto->SerializeAsString(),
                           url, max_retries, std::move(response_cb));
}

void FakeTachyonAuthedClient::StartAuthedRequestString(
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    std::string request_string,
    std::string_view url,
    int max_retries,
    std::unique_ptr<ResponseCallbackWrapper> response_cb) {
  has_new_request_ = true;
  response_cb_ = std::move(response_cb);
  request_string_ = std::move(request_string);
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void FakeTachyonAuthedClient::ExecuteResponseCallback(
    base::expected<std::string, ResponseCallbackWrapper::TachyonRequestError>
        response) {
  CHECK(response_cb_);
  response_cb_->Run(std::move(response));
}

std::string FakeTachyonAuthedClient::GetRequestString() {
  return request_string_;
}

void FakeTachyonAuthedClient::WaitForRequest() {
  if (!has_new_request_) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }
  has_new_request_ = false;
}

}  // namespace ash::babelorca
