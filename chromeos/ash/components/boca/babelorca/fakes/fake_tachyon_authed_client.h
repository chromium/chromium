// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_AUTHED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_AUTHED_CLIENT_H_

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"

namespace ash::babelorca {

class TachyonResponse;

class FakeTachyonAuthedClient : public TachyonAuthedClient {
 public:
  FakeTachyonAuthedClient();

  FakeTachyonAuthedClient(const FakeTachyonAuthedClient&) = delete;
  FakeTachyonAuthedClient& operator=(const FakeTachyonAuthedClient&) = delete;

  ~FakeTachyonAuthedClient() override;

  // TachyonAuthedClient:
  void StartAuthedRequest(
      std::unique_ptr<RequestDataWrapper> request_data,
      std::unique_ptr<google::protobuf::MessageLite> request_proto) override;
  void StartAuthedRequestString(
      std::unique_ptr<RequestDataWrapper> request_data,
      std::string request_string) override;

  void ExecuteResponseCallback(TachyonResponse response);

  RequestDataWrapper::ResponseCallback TakeResponseCallback();

  std::string GetRequestString();

  void WaitForRequest();

 private:
  RequestDataWrapper::ResponseCallback response_cb_;
  std::string request_string_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool has_new_request_ = false;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_AUTHED_CLIENT_H_
