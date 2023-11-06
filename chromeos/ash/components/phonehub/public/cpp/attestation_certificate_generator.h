// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PUBLIC_CPP_ATTESTATION_CERTIFICATE_GENERATOR_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PUBLIC_CPP_ATTESTATION_CERTIFICATE_GENERATOR_H_

#include <string>
#include <vector>
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash::phonehub {

// Generates attestation certificates for cross-device communication.
class AttestationCertificateGenerator {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Is executed in response to RetrieveCertificate() calls as well as
    // any time a new certificate is successfully generated automatically.
    virtual void OnCertificateGenerated(const std::vector<std::string>& certs,
                                        bool is_valid) = 0;
  };

  AttestationCertificateGenerator();
  virtual ~AttestationCertificateGenerator();
  AttestationCertificateGenerator(const AttestationCertificateGenerator&) =
      delete;
  AttestationCertificateGenerator& operator=(
      const AttestationCertificateGenerator&) = delete;

  // The certificate is provided through Observer::OnCertificateGenerated, be
  // sure to register an Observer before calling.
  virtual void RetrieveCertificate() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyCertificateGenerated(const std::vector<std::string>& certs,
                                  bool is_valid);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PUBLIC_CPP_ATTESTATION_CERTIFICATE_GENERATOR_H_
