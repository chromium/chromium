// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_ADAPTIVE_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_ADAPTIVE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/attestation_flow_factory.h"
#include "chromeos/ash/components/attestation/attestation_flow_status_reporter.h"
#include "chromeos/ash/components/attestation/attestation_flow_type_decider.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"

namespace ash {
namespace attestation {

// An attestation flow that adaptively chooses the preferred attestation flow
// object to perform the attestation flow, and falls back to the legacy
// attestation if the default (platform-side integrated) attestation flow fails.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ATTESTATION)
    AttestationFlowAdaptive : public AttestationFlow {
 public:
  explicit AttestationFlowAdaptive(std::unique_ptr<ServerProxy> server_proxy);

  // A constructor that injects the type decider and the factory. Used for
  // testing.
  AttestationFlowAdaptive(
      std::unique_ptr<ServerProxy> server_proxy,
      std::unique_ptr<AttestationFlowTypeDecider> type_decider,
      std::unique_ptr<AttestationFlowFactory> factory);

  ~AttestationFlowAdaptive() override;

  void GetCertificate(
      AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      bool force_new_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const std::optional<AttestationFlow::CertProfileSpecificData>&
          profile_specific_data,
      CertificateCallback callback) override;

 private:
  // The collection of parameters of `GetCertificate()` except for the callback.
  struct GetCertificateParams;

  // Called when the validity of the default attestation flow is checked.
  // Performs actions for verbosity, e.g., logging, before invoking
  // `StartGetCertificate()`.
  void OnCheckAttestationFlowType(
      const GetCertificateParams& params,
      std::unique_ptr<AttestationFlowStatusReporter> status_reporter,
      CertificateCallback callback,
      bool is_integrated_flow_possible);

  // Starts the default attestation flow if valid, otherwise just use the
  // fallback flow.
  void StartGetCertificate(
      const GetCertificateParams& params,
      std::unique_ptr<AttestationFlowStatusReporter> status_reporter,
      CertificateCallback callback,
      bool is_default_flow_valid);

  // Initialize the factory if needed. This can be called multiple times. This
  // function is designed to be idempotent.
  void InitializeAttestationFlowFactory();

  // Called when the default attestation flow returns the result. Runs
  // `callback` if the flow succeeds; otherwise, try the fallback.
  void OnGetCertificateWithDefaultFlow(
      const GetCertificateParams& params,
      std::unique_ptr<AttestationFlowStatusReporter> status_reporter,
      CertificateCallback callback,
      AttestationStatus status,
      const std::string& pem_certificate_chain);

  // Called when the fallback flow returns the result.
  void OnGetCertificateWithFallbackFlow(
      std::unique_ptr<AttestationFlowStatusReporter> status_reporter,
      CertificateCallback callback,
      AttestationStatus status,
      const std::string& pem_certificate_chain);

  // If `true`, the `InitializeAttestationFlowFactory()` performs no-ops.
  bool is_attestation_flow_initialized_ = false;

  // `ServerProxy` object passed during construction. Used by the type decider
  // to gather proxy information, and the attestation flow factory to
  // initialize.
  std::unique_ptr<ServerProxy> server_proxy_;
  // Owened by either `server_proxy_` or `attestation_flow_factory_`.
  const raw_ptr<ServerProxy, DanglingUntriaged> raw_server_proxy_;

  // `AttestationFlowTypeDecider` object that decides which attestation flow
  // type we can use.
  std::unique_ptr<AttestationFlowTypeDecider> attestation_flow_type_decider_;

  // The factory that creates the attestation flow objects.
  std::unique_ptr<AttestationFlowFactory> attestation_flow_factory_;

  base::WeakPtrFactory<AttestationFlowAdaptive> weak_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_ADAPTIVE_H_
