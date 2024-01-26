// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/certificate/cast_cert_printer.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "net/cert/x509_certificate.h"

namespace cast_certificate {

std::string CastCertificateChainAsPEM(const std::vector<std::string>& certs) {
  std::string result;
  std::string pem_encoded;
  for (auto cert : certs) {
    bool success =
        net::X509Certificate::GetPEMEncodedFromDER(cert, &pem_encoded);
    if (success) {
      base::StrAppend(&result, {pem_encoded});
    } else {
      base::StrAppend(&result, {"*** ERROR DECODING CERTIFICATE ***"});
    }
  }
  return result;
}

}  // namespace cast_certificate
