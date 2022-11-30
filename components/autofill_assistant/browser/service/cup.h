// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_CUP_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_CUP_H_

#include "components/autofill_assistant/browser/service/rpc_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

namespace cup {

// Whether |PackAndSignRequest| should be called before the request is
// submitted. Can be |false| because signing is disabled via feature flag,
// or given message type doesn't support CUP signing.
bool ShouldSignRequests(RpcType rpc_type);

// Whether |UnpackResponse| should be called on the response from the service
// call. Can be false because verification is disabled via feature flag or
// |ShouldSignRequest| returns |false|.
bool ShouldVerifyResponses(RpcType rpc_type);

// Whether this CUP implementation supports a given |rpc_type|.
bool IsRpcTypeSupported(RpcType rpc_type);

class CUP {
 public:
  virtual ~CUP() = default;

  // Generates a new |request| where |original_request| is packed and signed in
  // its |cup_data| field.
  virtual std::string PackAndSignRequest(
      const std::string& original_request) = 0;

  // Generates a new |response| where |original_response| is unpacked from
  // the |cup_data| field.
  virtual absl::optional<std::string> UnpackResponse(
      const std::string& original_response) = 0;

 protected:
  CUP() = default;
};

}  // namespace cup

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_CUP_H_
