// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_certificate_handler.h"

#include "base/observer_list_threadsafe.h"
#include "base/strings/stringprintf.h"
#include "chromeos/network/certificate_helper.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_util_nss.h"

namespace chromeos {

namespace {

// Root CA certificates that are built into Chrome use this token name.
const char kRootCertificateTokenName[] = "Builtin Object Token";

NetworkCertificateHandler::Certificate GetCertificate(CERTCertificate* cert,
                                                      net::CertType type) {
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
    OnCertificatesLoaded(NetworkCertLoader::Get()->all_certs());
}

NetworkCertificateHandler::~NetworkCertificateHandler() {
  NetworkCertLoader::Get()->RemoveObserver(this);
}

void NetworkCertificateHandler::AddObserver(
    NetworkCertificateHandler::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void NetworkCertificateHandler::RemoveObserver(
    NetworkCertificateHandler::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void NetworkCertificateHandler::OnCertificatesLoaded(
    const net::ScopedCERTCertificateList& cert_list) {
  ProcessCertificates(cert_list);
}

void NetworkCertificateHandler::ProcessCertificates(
    const net::ScopedCERTCertificateList& cert_list) {
  user_certificates_.clear();
  server_ca_certificates_.clear();

  // Add certificates to the appropriate list.
  for (const auto& cert_ref : cert_list) {
    CERTCertificate* cert = cert_ref.get();
    net::CertType type = certificate::GetCertType(cert);
    switch (type) {
      case net::USER_CERT:
        user_certificates_.push_back(GetCertificate(cert, type));
        break;
      case net::CA_CERT: {
        // Exclude root CA certificates that are built into Chrome.
        std::string token_name = certificate::GetCertTokenName(cert);
        if (token_name != kRootCertificateTokenName)
          server_ca_certificates_.push_back(GetCertificate(cert, type));
        else
          VLOG(2) << "Ignoring root cert";
        break;
      }
      default:
        // Ignore other certificates.
        VLOG(2) << "Ignoring cert type: " << type;
        break;
    }
  }

  for (auto& observer : observer_list_)
    observer.OnCertificatesChanged();
}

void NetworkCertificateHandler::SetCertificatesForTest(
    const net::ScopedCERTCertificateList& cert_list) {
  ProcessCertificates(cert_list);
}

void NetworkCertificateHandler::NotifyCertificatsChangedForTest() {
  for (auto& observer : observer_list_)
    observer.OnCertificatesChanged();
}

}  // namespace chromeos
