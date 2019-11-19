// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/mojom/proxy_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    cups_proxy::mojom::HttpHeaderDataView,
    ipp_converter::HttpHeader>::Read(cups_proxy::mojom::HttpHeaderDataView data,
                                     ipp_converter::HttpHeader* out_header) {
  return data.ReadKey(&out_header->first) &&
         data.ReadValue(&out_header->second);
}

}  // namespace mojo
