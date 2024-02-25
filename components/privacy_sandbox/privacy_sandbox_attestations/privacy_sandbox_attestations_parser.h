// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_PARSER_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_PARSER_H_

#include <optional>
#include <string>

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
namespace privacy_sandbox {

// Parse an `input` containing a serialized protobuf attestations list
// (`PrivacySandboxAttestationsProto`) and convert it into a C++
// `PrivacySandboxAttestationsMap`.
// Return std::nullopt if there is a catastrophic failure with parsing. This
// should never happen in normal use, but we should gracefully handle failure
// just in case the list sent from the server (or the file stored on disk) gets
// corrupted.
std::optional<PrivacySandboxAttestationsMap> ParseAttestationsFromString(
    std::string& input);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_PARSER_H_
