// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ash::babelorca {

FakeTachyonAuthedClient::FakeTachyonAuthedClient() = default;

FakeTachyonAuthedClient::~FakeTachyonAuthedClient() = default;

void FakeTachyonAuthedClient::StartAuthedRequest(
    std::unique_ptr<RequestDataWrapper> request_data,
    std::unique_ptr<google::protobuf::MessageLite> request_proto) {
  StartAuthedRequestString(std::move(request_data),
                           request_proto->SerializeAsString());
}

void FakeTachyonAuthedClient::StartAuthedRequestString(
    std::unique_ptr<RequestDataWrapper> request_data,
    std::string request_string) {
  has_new_request_ = true;
  response_cb_ = std::move(request_data->response_cb);
  request_string_ = std::move(request_string);
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void FakeTachyonAuthedClient::ExecuteResponseCallback(
    TachyonResponse response) {
  CHECK(response_cb_);
  std::move(response_cb_).Run(std::move(response));
}

RequestDataWrapper::ResponseCallback
FakeTachyonAuthedClient::TakeResponseCallback() {
  return std::move(response_cb_);
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
