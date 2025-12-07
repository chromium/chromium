// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_ATTESTATION_HANDLER_IMPL_H_
#define COMPONENTS_LEGION_ATTESTATION_HANDLER_IMPL_H_

#include "components/legion/attestation_handler.h"

namespace legion {

class AttestationHandlerImpl : public AttestationHandler {
 public:
  AttestationHandlerImpl();
  ~AttestationHandlerImpl() override;

  // AttestationHandler:
  std::optional<oak::session::v1::AttestRequest> GetAttestationRequest()
      override;
  bool VerifyAttestationResponse(
      const oak::session::v1::AttestResponse& evidence) override;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_ATTESTATION_HANDLER_IMPL_H_
