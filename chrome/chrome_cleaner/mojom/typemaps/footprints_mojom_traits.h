// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_FOOTPRINTS_MOJOM_TRAITS_H_
#define CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_FOOTPRINTS_MOJOM_TRAITS_H_

#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "chrome/chrome_cleaner/mojom/footprints.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<chrome_cleaner::mojom::FilePathDataView, base::FilePath> {
  static base::span<const uint16_t> value(const base::FilePath& file_path);
  static bool Read(chrome_cleaner::mojom::FilePathDataView path_view,
                   base::FilePath* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::RegistryKeyDataView, std::wstring> {
  static base::span<const uint16_t> value(const std::wstring& registry_key);
  static bool Read(chrome_cleaner::mojom::RegistryKeyDataView registry_key_view,
                   std::wstring* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::ExtensionIdDataView, std::wstring> {
  static base::span<const uint16_t> value(const std::wstring& extension_id);
  static bool Read(chrome_cleaner::mojom::ExtensionIdDataView extension_id_view,
                   std::wstring* out);
};

}  // namespace mojo

#endif  // CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_FOOTPRINTS_MOJOM_TRAITS_H_
