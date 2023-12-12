// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_parser.h"

#include <stdlib.h>
#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace {

class Environment {
 public:
  Environment() { CHECK(base::i18n::InitializeICU()); }

 private:
  base::AtExitManager at_exit_manager;  // Used by ICU integration.
};

// Use a binary proto fuzzer as the Privacy Sandbox Attestation file is a binary
// file.
DEFINE_BINARY_PROTO_FUZZER(
    const privacy_sandbox::PrivacySandboxAttestationsProto&
        attestations_proto) {
  static Environment env;

  std::string native_input = attestations_proto.SerializeAsString();

  if (getenv("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << native_input << std::endl;
  }

  privacy_sandbox::ParseAttestationsFromString(native_input);
}

}  // namespace
