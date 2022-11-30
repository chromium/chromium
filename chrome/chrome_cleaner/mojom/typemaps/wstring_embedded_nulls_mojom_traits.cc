// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/mojom/typemaps/wstring_embedded_nulls_mojom_traits.h"

#include "mojo/public/cpp/bindings/array_data_view.h"

namespace mojo {

using chrome_cleaner::WStringEmbeddedNulls;
using chrome_cleaner::mojom::WStringEmbeddedNullsDataView;

// static
base::span<const uint16_t>
StructTraits<WStringEmbeddedNullsDataView, WStringEmbeddedNulls>::value(
    const WStringEmbeddedNulls& str) {
  // This should only be called by Mojo to get the data to be send through the
  // pipe. When called by Mojo in this case, str will outlive the returned span.
  return base::make_span(str.CastAsUInt16Array(), str.size());
}

// static
bool StructTraits<WStringEmbeddedNullsDataView, WStringEmbeddedNulls>::Read(
    WStringEmbeddedNullsDataView str_view,
    WStringEmbeddedNulls* out) {
  ArrayDataView<uint16_t> view;
  str_view.GetValueDataView(&view);
  // Note: Casting is intentional, since the data view represents the string as
  //       a uint16_t array, whereas WStringEmbeddedNulls's constructor expects
  //       a wchar_t array.
  *out = WStringEmbeddedNulls(reinterpret_cast<const wchar_t*>(view.data()),
                              view.size());
  return true;
}

}  // namespace mojo
