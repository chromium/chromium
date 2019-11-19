// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_STRING16_EMBEDDED_NULLS_MOJOM_TRAITS_H_
#define CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_STRING16_EMBEDDED_NULLS_MOJOM_TRAITS_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "chrome/chrome_cleaner/mojom/string16_embedded_nulls.mojom.h"
#include "chrome/chrome_cleaner/strings/string16_embedded_nulls.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

namespace mojo {

// Defines NullValue as mapped to nullptr_t.
template <>
struct StructTraits<chrome_cleaner::mojom::NullValueDataView, nullptr_t> {
  static bool Read(chrome_cleaner::mojom::NullValueDataView data,
                   nullptr_t* value);
};

template <>
struct UnionTraits<chrome_cleaner::mojom::String16EmbeddedNullsDataView,
                   chrome_cleaner::String16EmbeddedNulls> {
  // This should only be called by Mojo to marshal the object before sending it
  // through the pipe.
  static base::span<const uint16_t> value(
      const chrome_cleaner::String16EmbeddedNulls& str);
  static nullptr_t null_value(const chrome_cleaner::String16EmbeddedNulls& str);

  static chrome_cleaner::mojom::String16EmbeddedNullsDataView::Tag GetTag(
      const chrome_cleaner::String16EmbeddedNulls& str);

  static bool Read(
      chrome_cleaner::mojom::String16EmbeddedNullsDataView str_view,
      chrome_cleaner::String16EmbeddedNulls* out);
};

}  // namespace mojo

#endif  // CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_STRING16_EMBEDDED_NULLS_MOJOM_TRAITS_H_
