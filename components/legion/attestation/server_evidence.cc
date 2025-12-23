// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/attestation/server_evidence.h"

namespace legion {

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

}  // namespace legion
