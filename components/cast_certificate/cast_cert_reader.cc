// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_certificate/cast_cert_reader.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "net/cert/internal/common_cert_errors.h"
#include "net/cert/pem.h"
#include "net/cert/x509_util.h"

namespace cast_certificate {

bool PopulateStoreWithCertsFromPath(net::TrustStoreInMemory* store,
                                    const base::FilePath& path) {
  const std::vector<std::string> trusted_roots =
      ReadCertificateChainFromFile(path);

  for (const auto& trusted_root : trusted_roots) {
    net::CertErrors errors;
    scoped_refptr<net::ParsedCertificate> cert(net::ParsedCertificate::Create(
        net::x509_util::CreateCryptoBuffer(trusted_root), {}, &errors));

    if (errors.ContainsAnyErrorWithSeverity(
            net::CertError::Severity::SEVERITY_HIGH)) {
      LOG(ERROR) << "Failed to load cert due to following error(s): "
                 << errors.ToDebugString();
      return false;
    }
    store->AddTrustAnchorWithConstraints(cert);
  }
  return true;
}

std::vector<std::string> ReadCertificateChainFromFile(
    const base::FilePath& path) {
  std::string file_data;
  if (!base::ReadFileToString(path, &file_data)) {
    LOG(ERROR) << "Failed to read certificate chain from file: " << path;
    return {};
  }

  std::vector<std::string> certs;
  net::PEMTokenizer pem_tokenizer(file_data, {"CERTIFICATE"});
  while (pem_tokenizer.GetNext())
    certs.push_back(pem_tokenizer.data());

  CHECK(!certs.empty());
  return certs;
}

}  // namespace cast_certificate
