// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_CUP_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_CUP_IMPL_H_

#include "components/autofill_assistant/browser/service/cup.h"
#include "components/client_update_protocol/ecdsa.h"

namespace autofill_assistant {

namespace cup {

// Implementation of the Client Update Protocol (CUP) for the service calls in
// |autofill_assistant|.
// https://source.chromium.org/chromium/chromium/src/+/main:docs/updater/cup.md
//
// Due to server-side constraints, the CUP information cannot be exchanged over
// HTTP headers, and is sent as part of the request and response body instead.
//
// This class can only be used once per service call.
class CUPImpl : public CUP {
 public:
  static std::string GetPublicKey();
  static int GetKeyVersion();
  static std::unique_ptr<client_update_protocol::Ecdsa> CreateQuerySigner();

  CUPImpl(std::unique_ptr<client_update_protocol::Ecdsa> query_signer,
          RpcType rpc_type);
  CUPImpl(const CUPImpl&) = delete;
  CUPImpl& operator=(const CUPImpl&) = delete;
  ~CUPImpl() override;

  // Generates a new |request| where |original_request| is packed and signed in
  // its |cup_data| field.
  //
  // Should only be called if |ShouldSignRequest| returns true.
  std::string PackAndSignRequest(const std::string& original_request) override;

  // Generates a new |response| where |original_response| is unpacked from
  // the |cup_data| field.
  //
  // Should only be called if |ShouldVerifyResponse| returns true.
  absl::optional<std::string> UnpackResponse(
      const std::string& original_response) override;

  // Gets the query signer object being used by this CUP instance. Needed for
  // testing.
  client_update_protocol::Ecdsa& GetQuerySigner();

 private:
  std::string PackGetActionsRequest(const std::string& original_request);

  absl::optional<std::string> UnpackGetActionsResponse(
      const std::string& original_response);

  std::unique_ptr<client_update_protocol::Ecdsa> query_signer_;
  RpcType rpc_type_;
};

}  // namespace cup

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_CUP_IMPL_H_
