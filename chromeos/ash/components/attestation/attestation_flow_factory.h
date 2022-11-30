// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"

namespace ash {
namespace attestation {

class AttestationFlow;
class ServerProxy;

// A factory that creates a default attestation flow we should try first and a
// fallback solution if necessary.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ATTESTATION)
    AttestationFlowFactory {
 public:
  AttestationFlowFactory();
  virtual ~AttestationFlowFactory();

  // Not copyable or movable.
  AttestationFlowFactory(const AttestationFlowFactory&) = delete;
  AttestationFlowFactory& operator=(const AttestationFlowFactory&) = delete;
  AttestationFlowFactory(AttestationFlowFactory&&) = delete;
  AttestationFlowFactory& operator=(AttestationFlowFactory&&) = delete;

  // Initializes the necessary step prior to invocation of `GetDefault()` and
  // `GetFallback()`. Can only be called once.
  virtual void Initialize(std::unique_ptr<ServerProxy> server_proxy);
  // Returns the default attestation flow.
  virtual AttestationFlow* GetDefault();
  // Returns the attestation flow used as a fallback solution.
  virtual AttestationFlow* GetFallback();

 private:
  // The `ServerProxy` object. This is initialized in `Initialize()`.
  std::unique_ptr<ServerProxy> server_proxy_;

  // The default attestation flow.
  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  // The fallback solution.
  std::unique_ptr<AttestationFlow> fallback_attestation_flow_;
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_FACTORY_H_
