// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_ATTESTATION_HANDLER_IMPL_H_
#define COMPONENTS_PRIVATE_AI_ATTESTATION_HANDLER_IMPL_H_

#include <map>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "components/private_ai/attestation/handler.h"
#include "components/private_ai/attestation/server_verification_key.h"
#include "components/private_ai/attestation/verification_key_utils.h"

namespace private_ai {

class PrivateAiLogger;

class AttestationHandlerImpl : public AttestationHandler {
 public:
  // Default constructor: Loads verification keys based on the environment.
  explicit AttestationHandlerImpl(PrivateAiLogger* logger);

  // Constructor for testing purposes, allowing injection of a pre-loaded
  // map of verification keys.
  AttestationHandlerImpl(PrivateAiLogger* logger,
                         std::map<uint32_t, VerificationKey> verification_keys);

  ~AttestationHandlerImpl() override;

  AttestationHandlerImpl(const AttestationHandlerImpl&) = delete;
  AttestationHandlerImpl& operator=(const AttestationHandlerImpl&) = delete;

  // AttestationHandler:
  std::optional<oak::session::v1::AttestRequest> GetAttestationRequest()
      override;
  bool VerifyAttestationResponse(const AttestationEvidence& evidence) override;

 private:
  raw_ptr<PrivateAiLogger> logger_;
  std::map<uint32_t, VerificationKey> verification_keys_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_ATTESTATION_HANDLER_IMPL_H_
