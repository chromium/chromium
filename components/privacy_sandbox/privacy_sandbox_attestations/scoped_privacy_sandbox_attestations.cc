// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"

namespace privacy_sandbox {

ScopedPrivacySandboxAttestations::ScopedPrivacySandboxAttestations(
    std::unique_ptr<PrivacySandboxAttestations> attestations)
    : attestations_(std::move(attestations)) {
  previous_attestations_ = PrivacySandboxAttestations::GetInstance();

  PrivacySandboxAttestations::SetInstanceForTesting(attestations_.get());
}

ScopedPrivacySandboxAttestations::~ScopedPrivacySandboxAttestations() {
  DCHECK_EQ(PrivacySandboxAttestations::GetInstance(), attestations_.get());

  PrivacySandboxAttestations::SetInstanceForTesting(previous_attestations_);
}

}  // namespace privacy_sandbox
