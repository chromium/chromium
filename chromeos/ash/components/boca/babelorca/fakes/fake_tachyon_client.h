// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client.h"

namespace ash::babelorca {

class FakeTachyonClient : public TachyonClient {
 public:
  FakeTachyonClient();

  FakeTachyonClient(const FakeTachyonClient&) = delete;
  FakeTachyonClient& operator=(const FakeTachyonClient&) = delete;

  ~FakeTachyonClient() override;

  // TachyonClient:
  void StartRequest(std::unique_ptr<RequestDataWrapper> request_data,
                    std::string oauth_token,
                    AuthFailureCallback auth_failure_cb) override;

  void WaitForRequest();
  RequestDataWrapper* GetRequestData();
  std::string GetOAuthToken();
  void ExecuteAuthFailCb();

 private:
  std::unique_ptr<RequestDataWrapper> request_data_;
  std::string oauth_token_;
  AuthFailureCallback auth_failure_cb_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_CLIENT_H_
