// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/record_replay/record_replay_mojom_traits.h"

#include "chrome/common/record_replay/aliases.h"

namespace mojo {

// static
bool StructTraits<record_replay::mojom::DomNodeIdDataView,
                  record_replay::DomNodeId>::
    Read(record_replay::mojom::DomNodeIdDataView data,
         record_replay::DomNodeId* out) {
  out->value() = data.id();
  return true;
}

// static
bool StructTraits<record_replay::mojom::FieldValueDataView,
                  record_replay::FieldValue>::
    Read(record_replay::mojom::FieldValueDataView data,
         record_replay::FieldValue* out) {
  return data.ReadValue(&out->value());
}

// static
bool StructTraits<
    record_replay::mojom::SelectorDataView,
    record_replay::Selector>::Read(record_replay::mojom::SelectorDataView data,
                                   record_replay::Selector* out) {
  return data.ReadSelector(&out->value());
}

}  // namespace mojo
