// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_STRICT_RELATIVE_PATH_MOJOM_TRAITS_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_STRICT_RELATIVE_PATH_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "components/services/storage/public/mojom/filesystem/directory.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(STORAGE_SERVICE_FILESYSTEM_TYPEMAP_TRAITS)
    StructTraits<storage::mojom::StrictRelativePathDataView, base::FilePath> {
 public:
  static const base::FilePath& path(const base::FilePath& path) { return path; }
  static bool Read(storage::mojom::StrictRelativePathDataView data,
                   base::FilePath* out);
};

}  // namespace mojo

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_STRICT_RELATIVE_PATH_MOJOM_TRAITS_H_
