// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/mojom/typemaps/footprints_mojom_traits.h"
#include "build/build_config.h"

namespace mojo {

// static
base::span<const uint16_t>
StructTraits<chrome_cleaner::mojom::FilePathDataView, base::FilePath>::value(
    const base::FilePath& file_path) {
#if BUILDFLAG(IS_WIN)
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
#if BUILDFLAG(IS_WIN)
  ArrayDataView<uint16_t> view;
  path_view.GetValueDataView(&view);
  base::FilePath path = base::FilePath(
      std::wstring(reinterpret_cast<const wchar_t*>(view.data()), view.size()));
  *out = std::move(path);
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

// static
base::span<const uint16_t>
StructTraits<chrome_cleaner::mojom::RegistryKeyDataView, std::wstring>::value(
    const std::wstring& registry_key) {
#if BUILDFLAG(IS_WIN)
  return base::make_span(reinterpret_cast<const uint16_t*>(registry_key.data()),
                         registry_key.size());
#else
  NOTREACHED();
  return base::span<const uint16_t>();
#endif
}

// static
bool StructTraits<chrome_cleaner::mojom::RegistryKeyDataView,
                  std::wstring>::Read(chrome_cleaner::mojom::RegistryKeyDataView
                                          registry_key_view,
                                      std::wstring* out) {
#if BUILDFLAG(IS_WIN)
  ArrayDataView<uint16_t> view;
  registry_key_view.GetValueDataView(&view);
  std::wstring registry_key =
      std::wstring(reinterpret_cast<const wchar_t*>(view.data()), view.size());
  *out = std::move(registry_key);
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

// static
base::span<const uint16_t>
StructTraits<chrome_cleaner::mojom::ExtensionIdDataView, std::wstring>::value(
    const std::wstring& extension_id) {
#if BUILDFLAG(IS_WIN)
  return base::make_span(reinterpret_cast<const uint16_t*>(extension_id.data()),
                         extension_id.size());
#else
  NOTREACHED();
  return base::span<const uint16_t>();
#endif
}

// static
bool StructTraits<chrome_cleaner::mojom::ExtensionIdDataView,
                  std::wstring>::Read(chrome_cleaner::mojom::ExtensionIdDataView
                                          extension_id_view,
                                      std::wstring* out) {
#if BUILDFLAG(IS_WIN)
  ArrayDataView<uint16_t> view;
  extension_id_view.GetValueDataView(&view);
  std::wstring extension_id =
      std::wstring(reinterpret_cast<const wchar_t*>(view.data()), view.size());
  *out = std::move(extension_id);
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

}  // namespace mojo
