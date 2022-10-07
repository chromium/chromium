// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/common/autofill_assistant_types_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<autofill_assistant::mojom::NodeDataDataView,
                  autofill_assistant::NodeData>::
    Read(autofill_assistant::mojom::NodeDataDataView data,
         autofill_assistant::NodeData* out) {
  out->backend_node_id = data.backend_node_id();
  out->used_override = data.used_override();
  return true;
}

}  // namespace mojo
