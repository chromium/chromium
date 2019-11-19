// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_FOOTPRINTS_MOJOM_TRAITS_H_
#define CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_FOOTPRINTS_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/mojom/footprints.mojom.h"

namespace mojo {

template <>
struct StructTraits<chrome_cleaner::mojom::FilePathDataView, base::FilePath> {
  static base::span<const uint16_t> value(const base::FilePath& file_path);
  static bool Read(chrome_cleaner::mojom::FilePathDataView path_view,
                   base::FilePath* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::RegistryKeyDataView,
                    base::string16> {
  static base::span<const uint16_t> value(const base::string16& registry_key);
  static bool Read(chrome_cleaner::mojom::RegistryKeyDataView registry_key_view,
                   base::string16* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::ExtensionIdDataView,
                    base::string16> {
  static base::span<const uint16_t> value(const base::string16& extension_id);
  static bool Read(chrome_cleaner::mojom::ExtensionIdDataView extension_id_view,
                   base::string16* out);
};

}  // namespace mojo

#endif  // CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_FOOTPRINTS_MOJOM_TRAITS_H_
