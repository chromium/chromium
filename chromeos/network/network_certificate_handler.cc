// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_certificate_handler.h"

#include "base/strings/stringprintf.h"
#include "chromeos/network/certificate_helper.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_util_nss.h"

namespace chromeos {

namespace {

NetworkCertificateHandler::Certificate GetCertificate(CERTCertificate* cert,
                                                      net::CertType type,
                                                      bool is_device_wide) {
  NetworkCertificateHandler::Certificate result;

  result.hash =
      net::HashValue(net::x509_util::CalculateFingerprint256(cert)).ToString();

  result.issued_by = certificate::GetIssuerDisplayName(cert);

  result.issued_to = certificate::GetCertNameOrNickname(cert);
  result.issued_to_ascii = certificate::GetCertAsciiNameOrNickname(cert);

  if (type == net::USER_CERT) {
    int slot_id;
    std::string pkcs11_id =
        NetworkCertLoader::GetPkcs11IdAndSlotForCert(cert, &slot_id);
    result.pkcs11_id = base::StringPrintf("%i:%s", slot_id, pkcs11_id.c_str());
  } else if (type == net::CA_CERT) {
    if (!net::x509_util::GetPEMEncoded(cert, &result.pem)) {
      LOG(ERROR) << "Unable to PEM-encode CA";
    }
  } else {
    NOTREACHED();
  }

  result.hardware_backed = NetworkCertLoader::IsCertificateHardwareBacked(cert);
  result.device_wide = is_device_wide;

  return result;
}

}  // namespace

NetworkCertificateHandler::Certificate::Certificate() = default;

NetworkCertificateHandler::Certificate::~Certificate() = default;

NetworkCertificateHandler::Certificate::Certificate(const Certificate& other) =
    default;

NetworkCertificateHandler::NetworkCertificateHandler() {
  NetworkCertLoader::Get()->AddObserver(this);
  if (NetworkCertLoader::Get()->initial_load_finished())
    OnCertificatesLoaded();
}

NetworkCertificateHandler::~NetworkCertificateHandler() {
  NetworkCertLoader::Get()->RemoveObserver(this);
}

void NetworkCertificateHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void NetworkCertificateHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool NetworkCertificateHandler::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

void NetworkCertificateHandler::AddAuthorityCertificateForTest(
    const std::string& issued_to) {
  Certificate cert;
  cert.issued_to = issued_to;
  cert.issued_to_ascii = issued_to;
  server_ca_certificates_.push_back(cert);
  for (auto& observer : observer_list_)
    observer.OnCertificatesChanged();
}

void NetworkCertificateHandler::OnCertificatesLoaded() {
  ProcessCertificates(NetworkCertLoader::Get()->authority_certs(),
                      NetworkCertLoader::Get()->client_certs());
}

void NetworkCertificateHandler::ProcessCertificates(
    const NetworkCertLoader::NetworkCertList& authority_certs,
    const NetworkCertLoader::NetworkCertList& client_certs) {
  client_certificates_.clear();
  server_ca_certificates_.clear();

  // Add certificates to the appropriate list.
  for (const auto& network_cert : authority_certs) {
    server_ca_certificates_.push_back(GetCertificate(
        network_cert.cert(), net::CA_CERT, network_cert.is_device_wide()));
  }
  for (const auto& network_cert : client_certs) {
    client_certificates_.push_back(GetCertificate(
        network_cert.cert(), net::USER_CERT, network_cert.is_device_wide()));
  }

  for (auto& observer : observer_list_)
    observer.OnCertificatesChanged();
}

}  // namespace chromeos
