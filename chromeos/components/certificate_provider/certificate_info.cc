// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/certificate_provider/certificate_info.h"

#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace certificate_provider {

CertificateInfo::CertificateInfo() {}

CertificateInfo::CertificateInfo(const CertificateInfo& other) = default;

CertificateInfo::~CertificateInfo() {}

bool CertificateInfo::operator==(const CertificateInfo& other) const {
  return net::X509Certificate::CalculateFingerprint256(
             this->certificate->cert_buffer()) ==
             net::X509Certificate::CalculateFingerprint256(
                 other.certificate->cert_buffer()) &&
         this->supported_algorithms == other.supported_algorithms;
}

}  // namespace certificate_provider
}  // namespace chromeos
