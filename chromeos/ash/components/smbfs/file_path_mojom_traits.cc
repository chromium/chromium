// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/smbfs/file_path_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<smbfs::mojom::FilePathDataView, base::FilePath>::Read(
    smbfs::mojom::FilePathDataView data,
    base::FilePath* out) {
  std::string path;
  if (!data.ReadPath(&path)) {
    return false;
  }

  base::FilePath file_path(path);
  if (path.compare(file_path.value()) != 0) {
    return false;
  }

  *out = file_path;
  return true;
}

}  // namespace mojo
