// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PUBLIC_MOJOM_PROXY_MOJOM_TRAITS_H_
#define CHROME_SERVICES_CUPS_PROXY_PUBLIC_MOJOM_PROXY_MOJOM_TRAITS_H_

#include "chrome/services/cups_proxy/public/mojom/proxy.mojom-shared.h"
#include "chrome/services/ipp_parser/public/cpp/ipp_converter.h"

namespace mojo {

template <>
class StructTraits<cups_proxy::mojom::HttpHeaderDataView,
                   ipp_converter::HttpHeader> {
 public:
  static const std::string& key(const ipp_converter::HttpHeader& header) {
    return header.first;
  }
  static const std::string& value(const ipp_converter::HttpHeader& header) {
    return header.second;
  }

  static bool Read(cups_proxy::mojom::HttpHeaderDataView data,
                   ipp_converter::HttpHeader* out_header);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_CUPS_PROXY_PUBLIC_MOJOM_PROXY_MOJOM_TRAITS_H_
