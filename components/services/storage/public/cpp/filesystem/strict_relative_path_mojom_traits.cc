// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/filesystem/strict_relative_path_mojom_traits.h"

#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"

namespace mojo {

bool StructTraits<storage::mojom::StrictRelativePathDataView, base::FilePath>::
    Read(storage::mojom::StrictRelativePathDataView data, base::FilePath* out) {
  base::FilePath path;
  if (!data.ReadPath(&path))
    return false;
  if (path.IsAbsolute() || path.ReferencesParent()) {
    DLOG(ERROR) << "Rejecting non-relative or non-descending path: "
                << path.value();
    return false;
  }

  *out = std::move(path);
  return true;
}

}  // namespace mojo
