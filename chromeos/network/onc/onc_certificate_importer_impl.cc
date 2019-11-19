// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_certificate_importer_impl.h"

#include <cert.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <stddef.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/onc/onc_parsed_certificates.h"
#include "chromeos/network/onc/onc_utils.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"

namespace chromeos {
namespace onc {

CertificateImporterImpl::CertificateImporterImpl(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    net::NSSCertDatabase* target_nssdb)
    : io_task_runner_(io_task_runner), target_nssdb_(target_nssdb) {
  CHECK(target_nssdb);
}

CertificateImporterImpl::~CertificateImporterImpl() = default;

void CertificateImporterImpl::ImportAllCertificatesUserInitiated(
    const std::vector<OncParsedCertificates::ServerOrAuthorityCertificate>&
        server_or_authority_certificates,
    const std::vector<OncParsedCertificates::ClientCertificate>&
        client_certificates,
    DoneCallback done_callback) {
  VLOG(2) << "Importing " << server_or_authority_certificates.size()
          << " server/authority certificates and " << client_certificates.size()
          << " client certificates";
  RunTaskOnIOTaskRunnerAndCallDoneCallback(
      base::BindOnce(&StoreAllCertificatesUserInitiated,
                     server_or_authority_certificates, client_certificates,
                     target_nssdb_),
      std::move(done_callback));
}

void CertificateImporterImpl::ImportClientCertificates(
    const std::vector<OncParsedCertificates::ClientCertificate>&
        client_certificates,
    DoneCallback done_callback) {
  VLOG(2) << "Permanently importing " << client_certificates.size()
          << " client certificates";
  RunTaskOnIOTaskRunnerAndCallDoneCallback(
      base::BindOnce(&StoreClientCertificates, client_certificates,
                     target_nssdb_),
      std::move(done_callback));
}

void CertificateImporterImpl::RunTaskOnIOTaskRunnerAndCallDoneCallback(
    base::OnceCallback<bool()> task,
    DoneCallback done_callback) {
  // Thereforce, call back to |this|. This check of |this| must happen last and
  // on the origin thread.
  DoneCallback callback_to_this =
      base::BindOnce(&CertificateImporterImpl::RunDoneCallback,
                     weak_factory_.GetWeakPtr(), std::move(done_callback));

  // The NSSCertDatabase must be accessed on |io_task_runner_|
  base::PostTaskAndReplyWithResult(io_task_runner_.get(), FROM_HERE,
                                   std::move(task),
                                   std::move(callback_to_this));
}

void CertificateImporterImpl::RunDoneCallback(DoneCallback callback,
                                              bool success) {
  if (!success)
    NET_LOG_ERROR("ONC Certificate Import Error", "");
  std::move(callback).Run(success);
}

// static
bool CertificateImporterImpl::StoreClientCertificates(
    const std::vector<OncParsedCertificates::ClientCertificate>&
        client_certificates,
    net::NSSCertDatabase* nssdb) {
  bool success = true;

  for (const OncParsedCertificates::ClientCertificate& client_certificate :
       client_certificates) {
    if (!StoreClientCertificate(client_certificate, nssdb)) {
      success = false;
    } else {
      VLOG(2) << "Successfully imported certificate with GUID "
              << client_certificate.guid();
    }
  }
  return success;
}

// static
bool CertificateImporterImpl::StoreAllCertificatesUserInitiated(
    const std::vector<OncParsedCertificates::ServerOrAuthorityCertificate>&
        server_or_authority_certificates,
    const std::vector<OncParsedCertificates::ClientCertificate>&
        client_certificates,
    net::NSSCertDatabase* nssdb) {
  bool success = true;

  for (const OncParsedCertificates::ServerOrAuthorityCertificate&
           server_or_authority_cert : server_or_authority_certificates) {
    if (!StoreServerOrCaCertificateUserInitiated(server_or_authority_cert,
                                                 nssdb)) {
      success = false;
    } else {
      VLOG(2) << "Successfully imported certificate with GUID "
              << server_or_authority_cert.guid();
    }
  }
  if (!StoreClientCertificates(client_certificates, nssdb))
    success = false;

  return success;
}

// static
bool CertificateImporterImpl::StoreServerOrCaCertificateUserInitiated(
    const OncParsedCertificates::ServerOrAuthorityCertificate& certificate,
    net::NSSCertDatabase* nssdb) {
  net::ScopedCERTCertificate x509_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(
          certificate.certificate().get());
  if (!x509_cert.get()) {
    LOG(ERROR) << "Unable to create certificate: " << certificate.guid();
    return false;
  }

  // Permanent web trust is granted to certificates imported by the user - and
  // StoreServerOrCaCertificateUserInitiated is only used if the user initiated
  // the import.
  net::NSSCertDatabase::TrustBits trust =
      (certificate.web_trust_requested() ? net::NSSCertDatabase::TRUSTED_SSL
                                         : net::NSSCertDatabase::TRUST_DEFAULT);

  if (x509_cert.get()->isperm) {
    net::CertType net_cert_type =
        certificate.type() == OncParsedCertificates::
                                  ServerOrAuthorityCertificate::Type::kServer
            ? net::SERVER_CERT
            : net::CA_CERT;
    VLOG(1) << "Certificate is already installed.";
    net::NSSCertDatabase::TrustBits missing_trust_bits =
        trust & ~nssdb->GetCertTrust(x509_cert.get(), net_cert_type);
    if (missing_trust_bits) {
      std::string error_reason;
      bool success = false;
      if (nssdb->IsReadOnly(x509_cert.get())) {
        error_reason = " Certificate is stored read-only.";
      } else {
        success = nssdb->SetCertTrust(x509_cert.get(), net_cert_type, trust);
      }
      if (!success) {
        LOG(ERROR) << "Certificate " << certificate.guid()
                   << " was already present, but trust couldn't be set."
                   << error_reason;
      }
    }
  } else {
    net::ScopedCERTCertificateList cert_list;
    cert_list.push_back(net::x509_util::DupCERTCertificate(x509_cert.get()));
    net::NSSCertDatabase::ImportCertFailureList failures;
    bool success = false;
    if (certificate.type() ==
        OncParsedCertificates::ServerOrAuthorityCertificate::Type::kServer)
      success = nssdb->ImportServerCert(cert_list, trust, &failures);
    else  // Authority cert
      success = nssdb->ImportCACerts(cert_list, trust, &failures);

    if (!failures.empty()) {
      std::string error_string = net::ErrorToString(failures[0].net_error);
      LOG(ERROR) << "Error ( " << error_string << " ) importing certificate "
                 << certificate.guid();
      return false;
    }

    if (!success) {
      LOG(ERROR) << "Unknown error importing certificate "
                 << certificate.guid();
      return false;
    }
  }

  return true;
}

// static
bool CertificateImporterImpl::StoreClientCertificate(
    const OncParsedCertificates::ClientCertificate& certificate,
    net::NSSCertDatabase* nssdb) {
  // Since this has a private key, always use the private module.
  crypto::ScopedPK11Slot private_slot(nssdb->GetPrivateSlot());
  if (!private_slot)
    return false;

  net::ScopedCERTCertificateList imported_certs;

  int import_result =
      nssdb->ImportFromPKCS12(private_slot.get(), certificate.pkcs12_data(),
                              base::string16(), false, &imported_certs);
  if (import_result != net::OK) {
    std::string error_string = net::ErrorToString(import_result);
    LOG(ERROR) << "Unable to import client certificate with guid "
               << certificate.guid() << ", error: " << error_string;
    return false;
  }

  if (imported_certs.size() == 0) {
    LOG(WARNING) << "PKCS12 data contains no importable certificates for guid "
                 << certificate.guid();
    return true;
  }

  if (imported_certs.size() != 1) {
    LOG(WARNING) << "PKCS12 data for guid " << certificate.guid()
                 << " contains more than one certificate. "
                    "Only the first one will be imported.";
  }

  CERTCertificate* cert_result = imported_certs[0].get();

  // Find the private key associated with this certificate, and set the
  // nickname on it. This is used by |ClientCertResolver| as a handle to resolve
  // onc ClientCertRef GUID references.
  SECKEYPrivateKey* private_key = PK11_FindPrivateKeyFromCert(
      cert_result->slot, cert_result, nullptr /* wincx */);
  if (private_key) {
    PK11_SetPrivateKeyNickname(private_key,
                               const_cast<char*>(certificate.guid().c_str()));
    SECKEY_DestroyPrivateKey(private_key);
  } else {
    LOG(WARNING) << "Unable to find private key for certificate "
                 << certificate.guid();
  }
  return true;
}

}  // namespace onc
}  // namespace chromeos
