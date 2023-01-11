// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_ATTESTATION_CERTIFICATES_SYNCER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_ATTESTATION_CERTIFICATES_SYNCER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace ash {
namespace device_sync {

// Uploads the attestation certs to cryptauth.
class AttestationCertificatesSyncer {
 public:
  using NotifyCallback =
      base::OnceCallback<void(const std::vector<std::string>&, bool valid)>;
  using GetAttestationCertificatesFunction =
      base::RepeatingCallback<void(const NotifyCallback, const std::string&)>;

  virtual ~AttestationCertificatesSyncer() = default;
  AttestationCertificatesSyncer(const AttestationCertificatesSyncer&) = delete;
  AttestationCertificatesSyncer& operator=(
      const AttestationCertificatesSyncer&) = delete;

  virtual bool IsUpdateRequired() = 0;
  // The timestamp is only updated on successful syncs of valid certificates (not mere attempts).
  virtual void SetLastSyncTimestamp() = 0;
  virtual void UpdateCerts(NotifyCallback callback,
                           const std::string& user_key) = 0;
  virtual void ScheduleSyncForTest() = 0;

 protected:
  AttestationCertificatesSyncer() = default;
};

}  // namespace device_sync
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_ATTESTATION_CERTIFICATES_SYNCER_H_
