// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow_factory.h"

#include <memory>
#include <utility>

#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/attestation_flow_integrated.h"

namespace ash {
namespace attestation {

AttestationFlowFactory::AttestationFlowFactory() = default;

AttestationFlowFactory::~AttestationFlowFactory() = default;

void AttestationFlowFactory::Initialize(
    std::unique_ptr<ServerProxy> server_proxy) {
  DCHECK(server_proxy.get());
  DCHECK(!server_proxy_);

  server_proxy_ = std::move(server_proxy);
}

AttestationFlow* AttestationFlowFactory::GetDefault() {
  if (!default_attestation_flow_) {
    default_attestation_flow_ = std::make_unique<AttestationFlowIntegrated>();
  }
  return default_attestation_flow_.get();
}

AttestationFlow* AttestationFlowFactory::GetFallback() {
  if (!fallback_attestation_flow_) {
    DCHECK(server_proxy_.get());
    fallback_attestation_flow_ =
        std::make_unique<AttestationFlowLegacy>(std::move(server_proxy_));
  }
  return fallback_attestation_flow_.get();
}
}  // namespace attestation
}  // namespace ash
