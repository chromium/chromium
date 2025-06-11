// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_NSS_H_
#define CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_NSS_H_

#include <string>

#include "chrome/common/net/x509_certificate_model.h"

typedef struct CERTCertificateStr CERTCertificate;

// This namespace defines a set of functions to be used in UI-related bits of
// X509 certificates.
namespace x509_certificate_model {

// Returns the commonName of the certificate, or if that is empty, returns the
// NSS certificate nickname (without the token name).
std::string GetCertNameOrNickname(CERTCertificate* cert_handle);

}  // namespace x509_certificate_model

#endif  // CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_NSS_H_
