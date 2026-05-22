// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_PUBLIC_MOJOM_ACTOR_TYPES_MOJOM_TRAITS_H_
#define COMPONENTS_ACTOR_PUBLIC_MOJOM_ACTOR_TYPES_MOJOM_TRAITS_H_

#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<actor::mojom::TaskIdDataView, actor::TaskId> {
  static int32_t id(const actor::TaskId& r) { return r.value(); }

  static bool Read(actor::mojom::TaskIdDataView data, actor::TaskId* out) {
    *out = actor::TaskId(data.id());
    return true;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_ACTOR_PUBLIC_MOJOM_ACTOR_TYPES_MOJOM_TRAITS_H_
