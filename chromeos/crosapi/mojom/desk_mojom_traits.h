// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_DESK_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_DESK_MOJOM_TRAITS_H_

#include "base/guid.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace base {
class GUID;
}

namespace mojo {

template <>
class StructTraits<crosapi::mojom::GUIDDataView, base::GUID> {
 public:
  static std::string lowercase(const base::GUID& guid);
  static bool Read(crosapi::mojom::GUIDDataView guid, base::GUID* out_guid);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_DESK_MOJOM_TRAITS_H_
