// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/crosapi/mojom/cert_database_mojom_traits.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace mojo {

using chromeos::certificate_provider::CertificateInfo;
using crosapi::mojom::CertInfoDataView;

std::vector<uint8_t> StructTraits<CertInfoDataView, CertificateInfo>::cert(
    const CertificateInfo& input) {
  CRYPTO_BUFFER* der_buffer = input.certificate->cert_buffer();
  const uint8_t* data = CRYPTO_BUFFER_data(der_buffer);
  return std::vector<uint8_t>(data, data + CRYPTO_BUFFER_len(der_buffer));
}

const std::vector<uint16_t>&
StructTraits<CertInfoDataView, CertificateInfo>::supported_algorithms(
    const CertificateInfo& input) {
  return input.supported_algorithms;
}

bool StructTraits<CertInfoDataView, CertificateInfo>::Read(
    crosapi::mojom::CertInfoDataView data,
    CertificateInfo* output) {
  std::vector<uint8_t> cert_der;
  if (!data.ReadCert(&cert_der))
    return false;
  net::X509Certificate::UnsafeCreateOptions options;
  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323 and crbug.com/788655.
  options.printable_string_is_utf8 = true;
  output->certificate =
      net::X509Certificate::CreateFromBytesUnsafeOptions(cert_der, options);
  // Intentionally leave output->certificate as nullptr on parse error,
  // as it is supposed to be filtered out separately later on.
  if (!data.ReadSupportedAlgorithms(&output->supported_algorithms))
    return false;
  return true;
}

}  // namespace mojo
