// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_WSTRING_EMBEDDED_NULLS_MOJOM_TRAITS_H_
#define CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_WSTRING_EMBEDDED_NULLS_MOJOM_TRAITS_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "chrome/chrome_cleaner/mojom/wstring_embedded_nulls.mojom-shared.h"
#include "chrome/chrome_cleaner/strings/wstring_embedded_nulls.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

namespace mojo {

template <>
struct StructTraits<chrome_cleaner::mojom::WStringEmbeddedNullsDataView,
                    chrome_cleaner::WStringEmbeddedNulls> {
  // This should only be called by Mojo to marshal the object before sending it
  // through the pipe.
  static base::span<const uint16_t> value(
      const chrome_cleaner::WStringEmbeddedNulls& str);

  static bool Read(chrome_cleaner::mojom::WStringEmbeddedNullsDataView str_view,
                   chrome_cleaner::WStringEmbeddedNulls* out);
};

}  // namespace mojo

#endif  // CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_WSTRING_EMBEDDED_NULLS_MOJOM_TRAITS_H_
