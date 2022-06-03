// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/mojom/typemaps/wstring_mojom_traits.h"

#include "mojo/public/cpp/bindings/array_data_view.h"

namespace mojo {

// static
bool StructTraits<chrome_cleaner::mojom::WStringDataView, std::wstring>::Read(
    chrome_cleaner::mojom::WStringDataView data,
    std::wstring* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  out->assign(reinterpret_cast<const wchar_t*>(view.data()), view.size());
  return true;
}

}  // namespace mojo
