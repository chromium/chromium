// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow_type_decider.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/attestation/attestation_flow.h"

#include "base/logging.h"

namespace chromeos {
namespace attestation {

AttestationFlowTypeDecider::AttestationFlowTypeDecider() = default;

AttestationFlowTypeDecider::~AttestationFlowTypeDecider() = default;

void AttestationFlowTypeDecider::CheckType(
    ServerProxy* server_proxy,
    AttestationFlowTypeCheckCallback callback) {
  server_proxy->CheckIfAnyProxyPresent(
      base::BindOnce(&AttestationFlowTypeDecider::OnCheckProxyPresence,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlowTypeDecider::OnCheckProxyPresence(
    AttestationFlowTypeCheckCallback callback,
    bool is_proxy_present) {
  // The integrated flow is currently only allowed if no proxy is present, until
  // the system-proxy daemon is enabled by default for pca_agentd.
  std::move(callback).Run(!is_proxy_present);
}

}  // namespace attestation
}  // namespace chromeos
