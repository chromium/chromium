// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"

namespace ash::babelorca {

FakeTachyonClient::FakeTachyonClient() = default;

FakeTachyonClient::~FakeTachyonClient() = default;

// TachyonClient:
void FakeTachyonClient::StartRequest(
    std::unique_ptr<RequestDataWrapper> request_data,
    std::string oauth_token,
    AuthFailureCallback auth_failure_cb) {
  request_data_ = std::move(request_data);
  oauth_token_ = std::move(oauth_token);
  auth_failure_cb_ = std::move(auth_failure_cb);
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void FakeTachyonClient::WaitForRequest() {
  if (request_data_) {
    return;
  }
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

RequestDataWrapper* FakeTachyonClient::GetRequestData() {
  return request_data_.get();
}

std::string FakeTachyonClient::GetOAuthToken() {
  return oauth_token_;
}

void FakeTachyonClient::ExecuteAuthFailCb() {
  std::move(auth_failure_cb_).Run(std::move(request_data_));
}

}  // namespace ash::babelorca
