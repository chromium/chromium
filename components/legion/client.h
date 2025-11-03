// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CLIENT_H_
#define COMPONENTS_LEGION_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/legion/legion_common.h"
#include "components/legion/secure_channel.h"

namespace legion {

// Client for starting the session and sending requests.
class Client {
 public:
  // Callback for when a `SendRequest` operation completes.
  // If the operation is successful, the result will contain the server's
  // response. Otherwise, it will contain an `ErrorCode` error.
  using OnRequestCompletedCallback =
      base::OnceCallback<void(base::expected<Response, ErrorCode> result)>;

  Client(
      std::unique_ptr<SecureChannel> secure_channel,
      const std::string& api_key);
  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  // Sends a request over the secure channel.
  // This method orchestrates the necessary steps:
  // 1. Authentication (e.g., using API Key)
  // 2. Calling the SecureChannel to send the request and receive the
  // response.
  // 3. Logging
  void SendRequest(Request request, OnRequestCompletedCallback callback);

 private:
  // Placeholder for Authentication using API Key.
  // Returns true on success, false on failure.
  bool Authenticate();

  const std::unique_ptr<SecureChannel> secure_channel_;
  const std::string api_key_;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_CLIENT_H_
