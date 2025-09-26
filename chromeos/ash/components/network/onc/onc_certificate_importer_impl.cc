// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_certificate_importer_impl.h"

#include <cert.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <stddef.h>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"

namespace ash::onc {

CertificateImporterImpl::CertificateImporterImpl(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    net::NSSCertDatabase* target_nssdb)
    : io_task_runner_(io_task_runner), target_nssdb_(target_nssdb) {
  CHECK(target_nssdb);
}

CertificateImporterImpl::~CertificateImporterImpl() = default;

void CertificateImporterImpl::ImportClientCertificates(
    const std::vector<chromeos::onc::OncParsedCertificates::ClientCertificate>&
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
  io_task_runner_->PostTaskAndReplyWithResult(FROM_HERE, std::move(task),
                                              std::move(callback_to_this));
}

void CertificateImporterImpl::RunDoneCallback(DoneCallback callback,
                                              bool success) {
  if (!success)
    NET_LOG(ERROR) << "ONC Certificate Import Error";
  std::move(callback).Run(success);
}

// static
bool CertificateImporterImpl::StoreClientCertificates(
    const std::vector<chromeos::onc::OncParsedCertificates::ClientCertificate>&
        client_certificates,
    net::NSSCertDatabase* nssdb) {
  bool success = true;

  for (const chromeos::onc::OncParsedCertificates::ClientCertificate&
           client_certificate : client_certificates) {
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
bool CertificateImporterImpl::StoreClientCertificate(
    const chromeos::onc::OncParsedCertificates::ClientCertificate& certificate,
    net::NSSCertDatabase* nssdb) {
  // Since this has a private key, always use the private module.
  crypto::ScopedPK11Slot private_slot(nssdb->GetPrivateSlot());
  if (!private_slot)
    return false;

  net::ScopedCERTCertificateList imported_certs;

  int import_result =
      nssdb->ImportFromPKCS12(private_slot.get(), certificate.pkcs12_data(),
                              std::u16string(), false, &imported_certs);
  if (import_result != net::OK) {
    std::string error_string = net::ErrorToString(import_result);
    NET_LOG(ERROR) << "Unable to import client certificate with guid: "
                   << certificate.guid() << ", error: " << error_string;
    return false;
  }

  if (imported_certs.size() == 0) {
    NET_LOG(ERROR)
        << "PKCS12 data contains no importable certificates for guid: "
        << certificate.guid();
    return true;
  }

  if (imported_certs.size() != 1) {
    NET_LOG(ERROR) << "PKCS12 data for guid: " << certificate.guid()
                   << " contains more than one certificate."
                   << " Only the first one will be imported.";
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
    NET_LOG(ERROR) << "Unable to find private key for certificate: "
                   << certificate.guid();
  }
  return true;
}

}  // namespace ash::onc
