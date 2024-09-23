// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_SCOPED_PRIVACY_SANDBOX_ATTESTATIONS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_SCOPED_PRIVACY_SANDBOX_ATTESTATIONS_H_

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"

#include <memory>

#include "base/memory/raw_ptr.h"

namespace privacy_sandbox {

// Helper class for tests. Initializes the `PrivacySandboxAttestations`
// singleton to the given |attestations| and tears it down again on destruction.
// If the singleton had already been initialized, its previous value is restored
// after tearing down |attestations|.
// TODO(crbug.com/41484063): Tests on Privacy Sandbox Attestation should disable
// default-allow feature. The default-allow behavior is only a temporary
// fix. The desired behavior is default-deny.
class ScopedPrivacySandboxAttestations {
 public:
  explicit ScopedPrivacySandboxAttestations(
      std::unique_ptr<PrivacySandboxAttestations> attestations);
  ~ScopedPrivacySandboxAttestations();

  ScopedPrivacySandboxAttestations(const ScopedPrivacySandboxAttestations&) =
      delete;
  ScopedPrivacySandboxAttestations& operator=(
      const ScopedPrivacySandboxAttestations&) = delete;

 private:
  const std::unique_ptr<PrivacySandboxAttestations> attestations_;
  raw_ptr<PrivacySandboxAttestations> previous_attestations_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_SCOPED_PRIVACY_SANDBOX_ATTESTATIONS_H_
