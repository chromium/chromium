// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/attestation_certificates_syncer_impl.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace device_sync {

namespace {

constexpr base::TimeDelta kValidTime = base::Hours(72);
constexpr base::TimeDelta kStartTimeout = base::Minutes(1);
constexpr base::TimeDelta kMaxTimeout = base::Hours(71);
constexpr base::TimeDelta kRegenerationThreshold = base::Hours(1);

}  // namespace

// static
AttestationCertificatesSyncerImpl::Factory*
    AttestationCertificatesSyncerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<AttestationCertificatesSyncer>
AttestationCertificatesSyncerImpl::Factory::Create(
    CryptAuthScheduler* cryptauth_scheduler,
    PrefService* pref_service,
    AttestationCertificatesSyncer::GetAttestationCertificatesFunction
        get_attestation_certificates_function) {
  if (!features::IsCryptauthAttestationSyncingEnabled()) {
    PA_LOG(WARNING)
        << "Attestation certificate generation not enabled, returning null";
    return nullptr;
  }
  if (test_factory_) {
    return test_factory_->CreateInstance(cryptauth_scheduler, pref_service,
                                         get_attestation_certificates_function);
  }
  return base::WrapUnique(new AttestationCertificatesSyncerImpl(
      cryptauth_scheduler, pref_service,
      get_attestation_certificates_function));
}

// static
void AttestationCertificatesSyncerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

AttestationCertificatesSyncerImpl::Factory::~Factory() = default;

// static
void AttestationCertificatesSyncerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  PA_LOG(INFO) << __func__;
  registry->RegisterTimePref(
      prefs::kCryptAuthAttestationCertificatesLastGeneratedTimestamp,
      base::Time() - kValidTime);
}

AttestationCertificatesSyncerImpl::AttestationCertificatesSyncerImpl(
    CryptAuthScheduler* cryptauth_scheduler,
    PrefService* pref_service,
    AttestationCertificatesSyncer::GetAttestationCertificatesFunction
        get_attestation_certificates_function)
    : cryptauth_scheduler_(cryptauth_scheduler),
      pref_service_(pref_service),
      get_attestation_certificates_function_(
          get_attestation_certificates_function) {
  StartTimer(kStartTimeout);
}

AttestationCertificatesSyncerImpl::~AttestationCertificatesSyncerImpl() =
    default;

void AttestationCertificatesSyncerImpl::UpdateCerts(
    NotifyCallback callback,
    const std::string& user_key) {
  DCHECK(features::IsCryptauthAttestationSyncingEnabled());

  PA_LOG(INFO) << __func__;

  // Cache the cert generation time, to be committed to prefs once the upload is
  // complete and SetLastSyncTimestamp is called.
  last_update_time_ = base::Time::Now();

  get_attestation_certificates_function_.Run(std::move(callback),
                                             std::move(user_key));
}

bool AttestationCertificatesSyncerImpl::IsUpdateRequired() {
  base::TimeDelta time_to_regeneration_threshold =
      CalculateTimeToRegeneration();
  return time_to_regeneration_threshold < base::Seconds(0);
}

void AttestationCertificatesSyncerImpl::SetLastSyncTimestamp() {
  PA_LOG(INFO) << __func__;
  pref_service_->SetTime(
      prefs::kCryptAuthAttestationCertificatesLastGeneratedTimestamp,
      last_update_time_);
}

void AttestationCertificatesSyncerImpl::ScheduleSyncForTest() {
  ScheduleSync();
}

base::TimeDelta
AttestationCertificatesSyncerImpl::CalculateTimeToRegeneration() {
  base::Time last_generated_time = pref_service_->GetTime(
      prefs::kCryptAuthAttestationCertificatesLastGeneratedTimestamp);
  return last_generated_time + kValidTime - base::Time::Now() -
         kRegenerationThreshold;
}

void AttestationCertificatesSyncerImpl::ScheduleSync() {
  DCHECK(features::IsCryptauthAttestationSyncingEnabled());

  PA_LOG(INFO) << "Checking attestation certificates status...";
  base::TimeDelta time_to_regeneration_threshold =
      CalculateTimeToRegeneration();
  if (time_to_regeneration_threshold < base::Seconds(0)) {
    PA_LOG(INFO) << "Requesting new attestation certificates sync";
    StartTimer(kMaxTimeout);
    cryptauth_scheduler_->RequestDeviceSync(
        cryptauthv2::ClientMetadata::InvocationReason::
            ClientMetadata_InvocationReason_FAST_PERIODIC,
        /*session_id=*/std::nullopt);
  } else {
    PA_LOG(INFO) << "Delaying new attestation certificate sync request";
    StartTimer(time_to_regeneration_threshold);
  }
}

void AttestationCertificatesSyncerImpl::StartTimer(base::TimeDelta timeout) {
  base::OnceClosure timeout_callback = base::BindOnce(
      &AttestationCertificatesSyncerImpl::ScheduleSync, base::Unretained(this));
  timer_.Start(FROM_HERE, timeout, std::move(timeout_callback));
}

}  // namespace device_sync
}  // namespace ash
