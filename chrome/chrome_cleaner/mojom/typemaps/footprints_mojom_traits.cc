// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/mojom/typemaps/footprints_mojom_traits.h"
#include "build/build_config.h"

namespace mojo {

// static
base::span<const uint16_t>
StructTraits<chrome_cleaner::mojom::FilePathDataView, base::FilePath>::value(
    const base::FilePath& file_path) {
#if defined(OS_WIN)
  return base::make_span(
      reinterpret_cast<const uint16_t*>(file_path.value().data()),
      file_path.value().size());
#else
  NOTREACHED();
  return base::span<const uint16_t>();
#endif
}

// static
bool StructTraits<chrome_cleaner::mojom::FilePathDataView,
                  base::FilePath>::Read(chrome_cleaner::mojom::FilePathDataView
                                            path_view,
                                        base::FilePath* out) {
#if defined(OS_WIN)
  ArrayDataView<uint16_t> view;
  path_view.GetValueDataView(&view);
  base::FilePath path = base::FilePath(base::string16(
      reinterpret_cast<const base::char16*>(view.data()), view.size()));
  *out = std::move(path);
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

// static
base::span<const uint16_t>
StructTraits<chrome_cleaner::mojom::RegistryKeyDataView, base::string16>::value(
    const base::string16& registry_key) {
#if defined(OS_WIN)
  return base::make_span(reinterpret_cast<const uint16_t*>(registry_key.data()),
                         registry_key.size());
#else
  NOTREACHED();
  return base::span<const uint16_t>();
#endif
}

// static
bool StructTraits<chrome_cleaner::mojom::RegistryKeyDataView, base::string16>::
    Read(chrome_cleaner::mojom::RegistryKeyDataView registry_key_view,
         base::string16* out) {
#if defined(OS_WIN)
  ArrayDataView<uint16_t> view;
  registry_key_view.GetValueDataView(&view);
  base::string16 registry_key = base::string16(
      reinterpret_cast<const base::char16*>(view.data()), view.size());
  *out = std::move(registry_key);
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

// static
base::span<const uint16_t>
StructTraits<chrome_cleaner::mojom::ExtensionIdDataView, base::string16>::value(
    const base::string16& extension_id) {
#if defined(OS_WIN)
  return base::make_span(reinterpret_cast<const uint16_t*>(extension_id.data()),
                         extension_id.size());
#else
  NOTREACHED();
  return base::span<const uint16_t>();
#endif
}

// static
bool StructTraits<chrome_cleaner::mojom::ExtensionIdDataView, base::string16>::
    Read(chrome_cleaner::mojom::ExtensionIdDataView extension_id_view,
         base::string16* out) {
#if defined(OS_WIN)
  ArrayDataView<uint16_t> view;
  extension_id_view.GetValueDataView(&view);
  base::string16 extension_id = base::string16(
      reinterpret_cast<const base::char16*>(view.data()), view.size());
  *out = std::move(extension_id);
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

}  // namespace mojo
