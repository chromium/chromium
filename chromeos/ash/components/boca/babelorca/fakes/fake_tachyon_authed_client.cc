// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
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
  response_cb_ = std::move(response_cb);
}

void FakeTachyonAuthedClient::ExecuteResponseCallback(
    base::expected<std::string, ResponseCallbackWrapper::TachyonRequestError>
        response) {
  CHECK(response_cb_);
  response_cb_->Run(std::move(response));
}

}  // namespace ash::babelorca
