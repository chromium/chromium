// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CERTIFICATE_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_CERTIFICATE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/network_cert_loader.h"

namespace chromeos {

// This class maintains user and server CA certificate lists for network
// configuration UI.
class CHROMEOS_EXPORT NetworkCertificateHandler
    : public NetworkCertLoader::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called for any Observers whenever the certificates are loaded and any
    // time the certificate lists change.
    virtual void OnCertificatesChanged() = 0;

   protected:
    Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  struct Certificate {
    Certificate();
    ~Certificate();
    Certificate(const Certificate& other);

    // A net::HashValue result used to uniquely identify certificates.
    std::string hash;

    // The X509 certificate issuer common name.
    std::string issued_by;

    // The X509 certificate common name or nickname.
    std::string issued_to;

    // The common name or nickname in Internationalized Domain Name format.
    std::string issued_to_ascii;

    // The PEM for Server CA certificates.
    std::string pem;

    // The PKCS#11 identifier in slot:id format for user certificates.
    std::string pkcs11_id;

    // True if a user certificate is stored in a hardware slot.
    bool hardware_backed = false;
  };

  NetworkCertificateHandler();
  ~NetworkCertificateHandler() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const std::vector<Certificate>& server_ca_certificates() const {
    return server_ca_certificates_;
  }
  const std::vector<Certificate>& user_certificates() const {
    return user_certificates_;
  }

  void SetCertificatesForTest(const net::ScopedCERTCertificateList& cert_list);
  void NotifyCertificatsChangedForTest();

 private:
  // NetworkCertLoader::Observer
  void OnCertificatesLoaded(
      const net::ScopedCERTCertificateList& cert_list) override;

  void ProcessCertificates(const net::ScopedCERTCertificateList& cert_list);

  base::ObserverList<NetworkCertificateHandler::Observer>::Unchecked
      observer_list_;

  std::vector<Certificate> server_ca_certificates_;
  std::vector<Certificate> user_certificates_;

  DISALLOW_COPY_AND_ASSIGN(NetworkCertificateHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CERTIFICATE_HANDLER_H_
