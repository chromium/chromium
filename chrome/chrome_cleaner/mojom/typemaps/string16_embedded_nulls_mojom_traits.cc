// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/mojom/typemaps/string16_embedded_nulls_mojom_traits.h"

namespace mojo {

using chrome_cleaner::mojom::NullValueDataView;
using chrome_cleaner::mojom::String16EmbeddedNullsDataView;
using chrome_cleaner::String16EmbeddedNulls;

// static
bool StructTraits<NullValueDataView, nullptr_t>::Read(NullValueDataView data,
                                                      nullptr_t* value) {
  *value = nullptr;
  return true;
}

// static
base::span<const uint16_t>
UnionTraits<String16EmbeddedNullsDataView, String16EmbeddedNulls>::value(
    const String16EmbeddedNulls& str) {
  DCHECK_EQ(String16EmbeddedNullsDataView::Tag::VALUE, GetTag(str));

  // This should only be called by Mojo to get the data to be send through the
  // pipe. When called by Mojo in this case, str will outlive the returned span.
  return base::make_span(str.CastAsUInt16Array(), str.size());
}

// static
nullptr_t
UnionTraits<String16EmbeddedNullsDataView, String16EmbeddedNulls>::null_value(
    const chrome_cleaner::String16EmbeddedNulls& str) {
  DCHECK_EQ(String16EmbeddedNullsDataView::Tag::NULL_VALUE, GetTag(str));

  return nullptr;
}

// static
chrome_cleaner::mojom::String16EmbeddedNullsDataView::Tag
UnionTraits<String16EmbeddedNullsDataView, String16EmbeddedNulls>::GetTag(
    const chrome_cleaner::String16EmbeddedNulls& str) {
  return str.size() == 0 ? String16EmbeddedNullsDataView::Tag::NULL_VALUE
                         : String16EmbeddedNullsDataView::Tag::VALUE;
}

// static
bool UnionTraits<String16EmbeddedNullsDataView, String16EmbeddedNulls>::Read(
    String16EmbeddedNullsDataView str_view,
    String16EmbeddedNulls* out) {
  if (str_view.is_null_value()) {
    *out = String16EmbeddedNulls();
    return true;
  }

  ArrayDataView<uint16_t> view;
  str_view.GetValueDataView(&view);
  // Note: Casting is intentional, since the data view represents the string as
  //       a uint16_t array, whereas String16EmbeddedNulls's constructor expects
  //       a wchar_t array.
  *out = String16EmbeddedNulls(reinterpret_cast<const wchar_t*>(view.data()),
                               view.size());
  return true;
}

}  // namespace mojo
