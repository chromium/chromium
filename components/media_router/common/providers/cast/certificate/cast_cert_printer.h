// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_PRINTER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_PRINTER_H_

#include <string>
#include <vector>

namespace cast_certificate {

// Returns a Cast device certificate chain as a PEM-encoded string, given
// `certs` which is a DER-encoded chain.  A valid chain will have the device
// certificate first in `certs`, followed by any intermediate certificate
// authorities, and finally ending with a Cast Root certificate.
//
// The resulting PEM-encoded string can be pretty printed using openssl:
// cat <output> | openssl x509 -inform pem -text -noout
std::string CastCertificateChainAsPEM(const std::vector<std::string>& certs);

}  // namespace cast_certificate

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_PRINTER_H_
