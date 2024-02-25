// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/certificate/cast_cert_reader.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/pki/common_cert_errors.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace cast_certificate {

bool PopulateStoreWithCertsFromPath(bssl::TrustStoreInMemory* store,
                                    const base::FilePath& path) {
  const std::vector<std::string> trusted_roots =
      ReadCertificateChainFromFile(path);

  for (const auto& trusted_root : trusted_roots) {
    bssl::CertErrors errors;
    std::shared_ptr<const bssl::ParsedCertificate> cert(
        bssl::ParsedCertificate::Create(
            net::x509_util::CreateCryptoBuffer(trusted_root), {}, &errors));

    if (errors.ContainsAnyErrorWithSeverity(
            bssl::CertError::Severity::SEVERITY_HIGH)) {
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

  return ReadCertificateChainFromString(file_data.data());
}

std::vector<std::string> ReadCertificateChainFromString(const char* str) {
  std::vector<std::string> certs;
  bssl::PEMTokenizer pem_tokenizer(str, {"CERTIFICATE"});
  while (pem_tokenizer.GetNext())
    certs.push_back(pem_tokenizer.data());

  if (certs.empty()) {
    LOG(WARNING) << "Certificate chain is empty.";
  }
  return certs;
}

}  // namespace cast_certificate
