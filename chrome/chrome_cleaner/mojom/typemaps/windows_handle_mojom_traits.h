// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_WINDOWS_HANDLE_MOJOM_TRAITS_H_
#define CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_WINDOWS_HANDLE_MOJOM_TRAITS_H_

#include "chrome/chrome_cleaner/mojom/windows_handle.mojom.h"
#include "mojo/public/cpp/bindings/union_traits.h"

namespace mojo {

template <>
struct EnumTraits<chrome_cleaner::mojom::PredefinedHandle, HANDLE> {
  static chrome_cleaner::mojom::PredefinedHandle ToMojom(HANDLE handle);
  static bool FromMojom(chrome_cleaner::mojom::PredefinedHandle input,
                        HANDLE* result);
};

template <>
struct UnionTraits<chrome_cleaner::mojom::WindowsHandleDataView, HANDLE> {
  static mojo::ScopedHandle raw_handle(HANDLE handle);
  static chrome_cleaner::mojom::PredefinedHandle special_handle(HANDLE handle);
  static chrome_cleaner::mojom::WindowsHandleDataView::Tag GetTag(
      HANDLE handle);
  static bool Read(
      chrome_cleaner::mojom::WindowsHandleDataView windows_handle_view,
      HANDLE* out);
};

}  // namespace mojo

#endif  // CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_WINDOWS_HANDLE_MOJOM_TRAITS_H_
