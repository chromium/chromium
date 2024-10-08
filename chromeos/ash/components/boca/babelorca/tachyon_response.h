// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_RESPONSE_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_RESPONSE_H_

#include <memory>
#include <optional>
#include <string>

namespace ash::babelorca {

class TachyonResponse {
 public:
  enum class Status {
    kOk,
    kHttpError,
    kNetworkError,
    kInternalError,
    kAuthError
  };
  explicit TachyonResponse(Status status);
  explicit TachyonResponse(int rpc_code, const std::string& error_message = "");
  TachyonResponse(int net_error,
                  std::optional<int> http_status_code,
                  std::unique_ptr<std::string> response_body);

  TachyonResponse(TachyonResponse&& other) = default;
  TachyonResponse& operator=(TachyonResponse&& other) = default;

  ~TachyonResponse();

  bool ok() const;

  Status status() const { return status_; }

  // Empty string if there is no response body.
  const std::string& response_body() const { return response_body_; }

  // Empty string if there is no error message.
  const std::string& error_message() const { return error_message_; }

 private:
  Status status_;
  std::string response_body_;
  std::string error_message_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_RESPONSE_H_
