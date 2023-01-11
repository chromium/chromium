// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "components/onc/onc_constants.h"
#include "net/cert/scoped_nss_types.h"

namespace ash::onc {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) CertificateImporter {
 public:
  // Called when certificate import is finished. |success| will be true if all
  // certificates which should be imported have been imported (or were already
  // in the database). |success| will be true if the import call has been made
  // with an empty list of certificates.
  typedef base::OnceCallback<void(bool success)> DoneCallback;

  CertificateImporter() {}

  CertificateImporter(const CertificateImporter&) = delete;
  CertificateImporter& operator=(const CertificateImporter&) = delete;

  virtual ~CertificateImporter() {}

  // This is intended for user-initiated ONC imports.
  // Permanently imports server, authority and client certificates from
  // |certificates|. Certificates will be given web trust if requested.
  // If the "Remove" field of a certificate is enabled, then removes the
  // certificate from the store instead of importing.
  // When the import is completed, |done_callback| will be called with |success|
  // equal to true if all certificates were imported successfully.
  // Never calls |done_callback| after this importer is destructed.
  virtual void ImportAllCertificatesUserInitiated(
      const std::vector<
          chromeos::onc::OncParsedCertificates::ServerOrAuthorityCertificate>&
          server_or_authority_certificates,
      const std::vector<
          chromeos::onc::OncParsedCertificates::ClientCertificate>&
          client_certificates,
      DoneCallback done_callback) = 0;

  // Permanently imports the client certificates given by |client_certificates|.
  // When the import is completed, |done_callback| will be called with |success|
  // equal to true if all certificates were imported successfully.
  // Never calls |done_callback| after this importer is destructed.
  virtual void ImportClientCertificates(
      const std::vector<
          chromeos::onc::OncParsedCertificates::ClientCertificate>&
          client_certificates,
      DoneCallback done_callback) = 0;
};

}  // namespace ash::onc

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_CERTIFICATE_IMPORTER_H_
