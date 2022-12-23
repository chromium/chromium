// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CERTIFICATE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CERTIFICATE_HANDLER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/network/network_cert_loader.h"

namespace ash {

// This class maintains user and server CA certificate lists for network
// configuration UI.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkCertificateHandler
    : public NetworkCertLoader::Observer {
 public:
  class Observer {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() {}

    // Called for any Observers whenever the certificates are loaded and any
    // time the certificate lists change.
    virtual void OnCertificatesChanged() = 0;

   protected:
    Observer() {}
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

    // True if a user certificate is stored in a slot that is available for
    // network authentication.
    bool available_for_network_auth = false;

    // True if a user certificate is stored in a hardware slot.
    bool hardware_backed = false;

    // True if the certificate is device-wide.
    bool device_wide = false;
  };

  NetworkCertificateHandler();

  NetworkCertificateHandler(const NetworkCertificateHandler&) = delete;
  NetworkCertificateHandler& operator=(const NetworkCertificateHandler&) =
      delete;

  ~NetworkCertificateHandler() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer);

  const std::vector<Certificate>& server_ca_certificates() const {
    return server_ca_certificates_;
  }
  const std::vector<Certificate>& client_certificates() const {
    return client_certificates_;
  }

  // Adds a testing certificate to the list of authority ceritificates and
  // notifies observers that certificates have been updated.
  void AddAuthorityCertificateForTest(const std::string& issued_to);

 private:
  // NetworkCertLoader::Observer
  void OnCertificatesLoaded() override;

  void ProcessCertificates(
      const NetworkCertLoader::NetworkCertList& authority_certs,
      const NetworkCertLoader::NetworkCertList& client_certs);

  base::ObserverList<NetworkCertificateHandler::Observer>::Unchecked
      observer_list_;

  std::vector<Certificate> server_ca_certificates_;
  std::vector<Certificate> client_certificates_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CERTIFICATE_HANDLER_H_
