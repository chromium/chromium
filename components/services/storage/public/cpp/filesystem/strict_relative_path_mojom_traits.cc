// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/filesystem/strict_relative_path_mojom_traits.h"

#include <algorithm>
#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"

namespace mojo {

bool StructTraits<storage::mojom::StrictRelativePathDataView, base::FilePath>::
    Read(storage::mojom::StrictRelativePathDataView data, base::FilePath* out) {
  base::FilePath path;
  if (!data.ReadPath(&path)) {
    return false;
  }
  if (path.IsAbsolute() || path.ReferencesParent()) {
    DLOG(ERROR) << "Rejecting non-relative or non-descending path: "
                << path.value();
    return false;
  }

#if BUILDFLAG(IS_WIN)
  if (std::ranges::any_of(path.GetComponents(),
                          &base::IsReservedNameOnWindows)) {
    DLOG(ERROR) << "Rejecting path containing reserved Windows device name: "
                << path.value();
    return false;
  }
#endif

  *out = std::move(path);
  return true;
}

}  // namespace mojo
