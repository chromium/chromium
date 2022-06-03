// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_WSTRING_MOJOM_TRAITS_H_
#define CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_WSTRING_MOJOM_TRAITS_H_

#include <string>

#include "base/containers/span.h"
#include "chrome/chrome_cleaner/mojom/wstring.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<chrome_cleaner::mojom::WStringDataView, std::wstring> {
  static base::span<const uint16_t> data(const std::wstring& str) {
    return base::make_span(reinterpret_cast<const uint16_t*>(str.data()),
                           str.size());
  }

  static bool Read(chrome_cleaner::mojom::WStringDataView data,
                   std::wstring* out);
};

}  // namespace mojo

#endif  // CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_WSTRING_MOJOM_TRAITS_H_
