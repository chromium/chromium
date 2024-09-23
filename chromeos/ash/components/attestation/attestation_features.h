// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FEATURES_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FEATURES_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace ash::attestation {

// AttestationFeatures maintains the attestation features, e.g. attestation
// availability, RSA/ECC support.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ATTESTATION)
    AttestationFeatures {
 public:
  using AttestationFeaturesCallback =
      base::OnceCallback<void(const AttestationFeatures* features)>;
  // Manage singleton instance.
  static void Initialize();
  static bool IsInitialized();
  static void Shutdown();
  static const AttestationFeatures* Get();

  // Run the |callback| with the pointer of the AttestationFeatures instance
  // after it is prepared completely. If we failed to prepare the
  // AttestationFatures, the pointer would be nullptr.
  static void GetFeatures(AttestationFeaturesCallback callback);

  // Sets the singleton to |test_instance|. Does not take ownership of the
  // instance. Should be matched with a call to |ShutdownForTesting| once the
  // test is finished and before the instance is deleted.
  static void SetForTesting(AttestationFeatures* test_instance);
  static void ShutdownForTesting();

  AttestationFeatures() = default;
  AttestationFeatures(const AttestationFeatures&) = delete;
  AttestationFeatures& operator=(const AttestationFeatures&) = delete;

  virtual ~AttestationFeatures() = default;

  virtual void Init() = 0;

  // Return the availability of the attestation service, e.g. preparation
  // enrollment, and certification.
  virtual bool IsAttestationAvailable() const = 0;

  // Returns if the RSA type of certified keys is supported.
  virtual bool IsRsaSupported() const = 0;

  // Returns if the ECC type of certified keys is supported.
  virtual bool IsEccSupported() const = 0;
};

}  // namespace ash::attestation

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FEATURES_H_
