// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_features.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"

namespace ash::attestation {

namespace {

AttestationFeatures* g_attestation_features = nullptr;
bool g_is_ready = false;

// Calling SetForTesting sets this flag. This flag means that the production
// code which calls Initialize and Shutdown will have no effect - the test
// install attributes will remain in place until ShutdownForTesting is called.
bool g_using_attestation_features_for_testing = false;

constexpr base::TimeDelta kPrepareFeaturesTimeout = base::Seconds(60);
constexpr base::TimeDelta kGetFeaturesTimeout = base::Seconds(60);
constexpr base::TimeDelta kRetryDelay = base::Seconds(1);

void GetFeaturesInternal(
    base::TimeTicks end_time,
    AttestationFeatures::AttestationFeaturesCallback callback) {
  if (g_is_ready) {
    std::move(callback).Run(g_attestation_features);
    return;
  }
  if (base::TimeTicks::Now() >= end_time) {
    std::move(callback).Run(nullptr);
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GetFeaturesInternal, end_time, std::move(callback)),
      kRetryDelay);
}

}  // namespace

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ATTESTATION)
    AttestationFeaturesImpl : public AttestationFeatures {
 public:
  explicit AttestationFeaturesImpl(AttestationClient* attestation_client);

  AttestationFeaturesImpl(const AttestationFeaturesImpl&) = delete;
  AttestationFeaturesImpl& operator=(const AttestationFeaturesImpl&) = delete;

  ~AttestationFeaturesImpl() override;

  void Init() override;

  bool IsAttestationAvailable() const override;
  bool IsRsaSupported() const override;
  bool IsEccSupported() const override;

 private:
  void OnAttestationServiceAvailable(bool service_is_ready);
  void PrepareFeatures(base::TimeTicks end_time);
  void OnPrepareFeaturesComplete(base::TimeTicks end_time,
                                 const ::attestation::GetFeaturesReply& reply);

  bool is_available_ = false;
  bool is_rsa_supported_ = false;
  bool is_ecc_supported_ = false;

  raw_ptr<AttestationClient, DanglingUntriaged> attestation_client_;
  base::WeakPtrFactory<AttestationFeaturesImpl> weak_factory_{this};
};

AttestationFeaturesImpl::AttestationFeaturesImpl(
    AttestationClient* attestation_client)
    : attestation_client_(attestation_client) {}
AttestationFeaturesImpl::~AttestationFeaturesImpl() = default;

// static
void AttestationFeatures::Initialize() {
  // Don't reinitialize if a specific instance has already been set for test.
  if (g_using_attestation_features_for_testing) {
    return;
  }

  DCHECK(!g_attestation_features);
  g_attestation_features =
      new AttestationFeaturesImpl(AttestationClient::Get());
  g_attestation_features->Init();
}

// static
bool AttestationFeatures::IsInitialized() {
  return g_attestation_features;
}

// static
void AttestationFeatures::Shutdown() {
  if (g_using_attestation_features_for_testing) {
    return;
  }

  DCHECK(g_attestation_features);
  delete g_attestation_features;
  g_is_ready = false;
  g_attestation_features = nullptr;
}

// static
const AttestationFeatures* AttestationFeatures::Get() {
  DCHECK(g_attestation_features);
  return g_attestation_features;
}

// static
void AttestationFeatures::GetFeatures(
    AttestationFeatures::AttestationFeaturesCallback callback) {
  if (g_attestation_features == nullptr) {
    LOG(ERROR) << "The attestation features haven't been initialized.";
    std::move(callback).Run(nullptr);
    return;
  }
  base::TimeTicks end_time = base::TimeTicks::Now() + kGetFeaturesTimeout;
  GetFeaturesInternal(end_time, std::move(callback));
}

// static
void AttestationFeatures::SetForTesting(AttestationFeatures* test_instance) {
  DCHECK(!g_attestation_features);
  DCHECK(!g_using_attestation_features_for_testing);
  g_attestation_features = test_instance;
  g_is_ready = true;
  g_using_attestation_features_for_testing = true;
}

// static
void AttestationFeatures::ShutdownForTesting() {
  DCHECK(g_using_attestation_features_for_testing);
  // Don't delete the test instance, we are not the owner.
  g_attestation_features = nullptr;
  g_is_ready = false;
  g_using_attestation_features_for_testing = false;
}

void AttestationFeaturesImpl::Init() {
  attestation_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&AttestationFeaturesImpl::OnAttestationServiceAvailable,
                     weak_factory_.GetWeakPtr()));
}

void AttestationFeaturesImpl::OnAttestationServiceAvailable(
    bool service_is_ready) {
  if (!service_is_ready) {
    LOG(ERROR) << "Failed waiting for Attestation D-Bus service availability.";
  }
  base::TimeTicks end_time = base::TimeTicks::Now() + kPrepareFeaturesTimeout;
  PrepareFeatures(end_time);
}

void AttestationFeaturesImpl::PrepareFeatures(base::TimeTicks end_time) {
  attestation_client_->GetFeatures(
      ::attestation::GetFeaturesRequest(),
      base::BindOnce(&AttestationFeaturesImpl::OnPrepareFeaturesComplete,
                     weak_factory_.GetWeakPtr(), end_time));
}

void AttestationFeaturesImpl::OnPrepareFeaturesComplete(
    base::TimeTicks end_time,
    const ::attestation::GetFeaturesReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to get features; status: "
               << reply.status();
    if (base::TimeTicks::Now() < end_time) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AttestationFeaturesImpl::PrepareFeatures,
                         weak_factory_.GetWeakPtr(), end_time),
          kRetryDelay);
    }
    return;
  }
  is_available_ = reply.is_available();
  for (auto supported_type : reply.supported_key_types()) {
    switch (supported_type) {
      case ::attestation::KEY_TYPE_RSA:
        is_rsa_supported_ = true;
        break;
      case ::attestation::KEY_TYPE_ECC:
        is_ecc_supported_ = true;
        break;
    }
  }
  g_is_ready = true;
}

bool AttestationFeaturesImpl::IsAttestationAvailable() const {
  return is_available_;
}

bool AttestationFeaturesImpl::IsRsaSupported() const {
  return is_rsa_supported_;
}

bool AttestationFeaturesImpl::IsEccSupported() const {
  return is_ecc_supported_;
}

}  // namespace ash::attestation
