// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_ATTESTATION_SERVER_EVIDENCE_H_
#define COMPONENTS_PRIVATE_AI_ATTESTATION_SERVER_EVIDENCE_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace private_ai {

// Contains a message and its corresponding signature, extracted from the
// server's attestation response. This is the core piece of information needed
// for signature verification.
struct Endorsement {
  Endorsement();
  Endorsement(const Endorsement&) = delete;
  Endorsement& operator=(const Endorsement&) = delete;
  Endorsement(Endorsement&&);
  Endorsement& operator=(Endorsement&&);
  ~Endorsement();

  std::vector<uint8_t> message;
  std::vector<uint8_t> signature;
};

struct EndorsedEvidence {
  EndorsedEvidence();
  EndorsedEvidence(const EndorsedEvidence&) = delete;
  EndorsedEvidence& operator=(const EndorsedEvidence&) = delete;
  EndorsedEvidence(EndorsedEvidence&&);
  EndorsedEvidence& operator=(EndorsedEvidence&&);
  ~EndorsedEvidence();

  // A collection of signed statements from the server.
  std::vector<Endorsement> endorsements;
};

struct AttestationEvidence {
  AttestationEvidence();
  AttestationEvidence(const AttestationEvidence&) = delete;
  AttestationEvidence& operator=(const AttestationEvidence&) = delete;
  AttestationEvidence(AttestationEvidence&&);
  AttestationEvidence& operator=(AttestationEvidence&&);
  ~AttestationEvidence();

  std::map<std::string, EndorsedEvidence> endorsed_evidence;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_ATTESTATION_SERVER_EVIDENCE_H_
