// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_MOJOM_TRAITS_H_
#define CHROME_COMMON_ACTOR_MOJOM_TRAITS_H_

#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
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

#endif  // CHROME_COMMON_ACTOR_MOJOM_TRAITS_H_
