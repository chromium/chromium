// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ash::babelorca {

struct RequestDataWrapper;

class TachyonClient {
 public:
  using AuthFailureCallback =
      base::OnceCallback<void(std::unique_ptr<RequestDataWrapper>)>;

  static void HandleResponse(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      std::unique_ptr<RequestDataWrapper> request_data,
      AuthFailureCallback auth_failure_cb,
      std::unique_ptr<std::string> response_body);

  TachyonClient(const TachyonClient&) = delete;
  TachyonClient& operator=(const TachyonClient&) = delete;

  virtual ~TachyonClient() = default;

  virtual void StartRequest(std::unique_ptr<RequestDataWrapper> request_data,
                            std::string oauth_token,
                            AuthFailureCallback auth_failure_cb) = 0;

 protected:
  TachyonClient() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_CLIENT_H_
