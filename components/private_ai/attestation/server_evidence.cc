// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/server_evidence.h"

namespace private_ai {

Endorsement::Endorsement() = default;
Endorsement::Endorsement(Endorsement&&) = default;
Endorsement& Endorsement::operator=(Endorsement&&) = default;
Endorsement::~Endorsement() = default;

EndorsedEvidence::EndorsedEvidence() = default;
EndorsedEvidence::EndorsedEvidence(EndorsedEvidence&&) = default;
EndorsedEvidence& EndorsedEvidence::operator=(EndorsedEvidence&&) = default;
EndorsedEvidence::~EndorsedEvidence() = default;

AttestationEvidence::AttestationEvidence() = default;
AttestationEvidence::AttestationEvidence(AttestationEvidence&&) = default;
AttestationEvidence& AttestationEvidence::operator=(AttestationEvidence&&) =
    default;
AttestationEvidence::~AttestationEvidence() = default;

}  // namespace private_ai
