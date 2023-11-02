// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_CERT_DATABASE_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_CERT_DATABASE_MOJOM_TRAITS_H_

#include "chromeos/components/certificate_provider/certificate_info.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<crosapi::mojom::CertInfoDataView,
                   chromeos::certificate_provider::CertificateInfo> {
  using CertificateInfo = chromeos::certificate_provider::CertificateInfo;

 public:
  static std::vector<uint8_t> cert(const CertificateInfo& input);
  static const std::vector<uint16_t>& supported_algorithms(
      const CertificateInfo& input);

  static bool Read(crosapi::mojom::CertInfoDataView data,
                   CertificateInfo* output);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_CERT_DATABASE_MOJOM_TRAITS_H_
