// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "components/onc/onc_constants.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class NSSCertDatabase;
}

namespace ash::onc {

// This class handles certificate imports from ONC (both policy and user
// imports) into a certificate store. The GUID of Client certificates is stored
// together with the certificate as Nickname. In contrast, Server and CA
// certificates are identified by their PEM and not by GUID.
// TODO(pneubeck): Replace Nickname by PEM for Client
// certificates. http://crbug.com/252119
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CertificateImporterImpl
    : public CertificateImporter {
 public:
  // |io_task_runner| will be used for NSSCertDatabase accesses.
  CertificateImporterImpl(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
      net::NSSCertDatabase* target_nssdb_);

  CertificateImporterImpl(const CertificateImporterImpl&) = delete;
  CertificateImporterImpl& operator=(const CertificateImporterImpl&) = delete;

  ~CertificateImporterImpl() override;

  // CertificateImporter overrides
  void ImportAllCertificatesUserInitiated(
      const std::vector<
          chromeos::onc::OncParsedCertificates::ServerOrAuthorityCertificate>&
          server_or_authority_certificates,
      const std::vector<
          chromeos::onc::OncParsedCertificates::ClientCertificate>&
          client_certificates,
      DoneCallback done_callback) override;

  void ImportClientCertificates(
      const std::vector<
          chromeos::onc::OncParsedCertificates::ClientCertificate>&
          client_certificates,
      DoneCallback done_callback) override;

 private:
  // Runs |task| on the |io_task_runner_|. Calls |done_callback| on the origin
  // loop if this |CertificateImporterImpl| has not been destroyed in the
  // meantime.
  void RunTaskOnIOTaskRunnerAndCallDoneCallback(base::OnceCallback<bool()> task,
                                                DoneCallback done_callback);

  // Calls |callback| with |success|. This is used to ensure that |callback| is
  // only called if this |CertificateImporterImpl| has not been destroyed yet.
  void RunDoneCallback(DoneCallback callback, bool success);

  // Synchronously imports |client_certificates| into |nssdb|. This will be
  // executed on the |io_task_runner_|.
  static bool StoreClientCertificates(
      const std::vector<
          chromeos::onc::OncParsedCertificates::ClientCertificate>&
          client_certificates,
      net::NSSCertDatabase* nssdb);

  // Synchronously imports all server/authority and client certificates from
  // |certificates| into |nssdb|. This will be executed on the
  // |io_task_runner_|.
  static bool StoreAllCertificatesUserInitiated(
      const std::vector<
          chromeos::onc::OncParsedCertificates::ServerOrAuthorityCertificate>&
          server_or_authority_certificates,
      const std::vector<
          chromeos::onc::OncParsedCertificates::ClientCertificate>&
          client_certificates,
      net::NSSCertDatabase* nssdb);

  // Imports the Server or CA certificate |certificate|. Web trust is only
  // applied if the certificate requests the TrustBits attribute "Web".
  static bool StoreServerOrCaCertificateUserInitiated(
      const chromeos::onc::OncParsedCertificates::ServerOrAuthorityCertificate&
          certificate,
      net::NSSCertDatabase* nssdb);

  static bool StoreClientCertificate(
      const chromeos::onc::OncParsedCertificates::ClientCertificate&
          certificate,
      net::NSSCertDatabase* nssdb);

  // The task runner to use for NSSCertDatabase accesses.
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // The certificate database to which certificates are imported.
  raw_ptr<net::NSSCertDatabase> target_nssdb_;

  base::WeakPtrFactory<CertificateImporterImpl> weak_factory_{this};
};

}  // namespace ash::onc

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_IMPL_H_
