// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_AUTHED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_AUTHED_CLIENT_H_

#include <memory>
#include <string_view>

#include "base/run_loop.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/response_callback_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"

namespace ash::babelorca {

class FakeTachyonAuthedClient : public TachyonAuthedClient {
 public:
  FakeTachyonAuthedClient();

  FakeTachyonAuthedClient(const FakeTachyonAuthedClient&) = delete;
  FakeTachyonAuthedClient& operator=(const FakeTachyonAuthedClient&) = delete;

  ~FakeTachyonAuthedClient() override;

  // TachyonAuthedClient:
  void StartAuthedRequest(
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      std::unique_ptr<google::protobuf::MessageLite> request_proto,
      std::string_view url,
      int max_retries,
      std::unique_ptr<ResponseCallbackWrapper> response_cb) override;
  void StartAuthedRequestString(
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      std::string request_string,
      std::string_view url,
      int max_retries,
      std::unique_ptr<ResponseCallbackWrapper> response_cb) override;

  void ExecuteResponseCallback(
      base::expected<std::string, ResponseCallbackWrapper::TachyonRequestError>
          response);

  std::string GetRequestString();

  void WaitForRequest();

 private:
  std::unique_ptr<ResponseCallbackWrapper> response_cb_;
  std::string request_string_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool has_new_request_ = false;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_AUTHED_CLIENT_H_
