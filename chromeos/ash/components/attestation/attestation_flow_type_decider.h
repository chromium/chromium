// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_TYPE_DECIDER_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_TYPE_DECIDER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace ash {
namespace attestation {

class AttestationFlowStatusReporter;
class ServerProxy;

// An object that decides if the default (platform-side integrated) flow is a
// valid option.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ATTESTATION)
    AttestationFlowTypeDecider {
 public:
  using AttestationFlowTypeCheckCallback =
      base::OnceCallback<void(bool is_integrated_flow_valid)>;

  AttestationFlowTypeDecider();
  virtual ~AttestationFlowTypeDecider();

  // Checks if the default attestation flow is a valid option.
  virtual void CheckType(ServerProxy* server_proxy,
                         AttestationFlowStatusReporter* reporter,
                         AttestationFlowTypeCheckCallback callback);

 private:
  // Called when `proxy_server` returns the check of proxy presence.
  void OnCheckProxyPresence(AttestationFlowStatusReporter* reporter,
                            AttestationFlowTypeCheckCallback callback,
                            bool is_proxy_present);

  base::WeakPtrFactory<AttestationFlowTypeDecider> weak_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_TYPE_DECIDER_H_
