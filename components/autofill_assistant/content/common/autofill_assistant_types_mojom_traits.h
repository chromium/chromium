// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_COMMON_AUTOFILL_ASSISTANT_TYPES_MOJOM_TRAITS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_COMMON_AUTOFILL_ASSISTANT_TYPES_MOJOM_TRAITS_H_

#include <stdint.h>

#include "components/autofill_assistant/content/common/autofill_assistant_types.mojom-shared.h"
#include "components/autofill_assistant/content/common/node_data.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<autofill_assistant::mojom::NodeDataDataView,
                    autofill_assistant::NodeData> {
  static int32_t backend_node_id(const autofill_assistant::NodeData& r) {
    return r.backend_node_id;
  }

  static bool used_override(const autofill_assistant::NodeData& r) {
    return r.used_override;
  }

  static bool Read(autofill_assistant::mojom::NodeDataDataView data,
                   autofill_assistant::NodeData* out);
};

}  // namespace mojo

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_COMMON_AUTOFILL_ASSISTANT_TYPES_MOJOM_TRAITS_H_
