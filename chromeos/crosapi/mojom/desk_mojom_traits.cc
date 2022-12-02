// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/desk_mojom_traits.h"
#include "base/guid.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"

namespace mojo {

// static
std::string StructTraits<crosapi::mojom::GUIDDataView, base::GUID>::lowercase(
    const base::GUID& guid) {
  return guid.AsLowercaseString();
}

bool StructTraits<crosapi::mojom::GUIDDataView, base::GUID>::Read(
    crosapi::mojom::GUIDDataView guid,
    base::GUID* out_guid) {
  std::string guid_lowercase;
  if (!guid.ReadLowercase(&guid_lowercase))
    return false;
  *out_guid = base::GUID::ParseCaseInsensitive(guid_lowercase);
  return true;
}

}  // namespace mojo
