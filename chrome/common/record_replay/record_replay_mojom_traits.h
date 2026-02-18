// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RECORD_REPLAY_RECORD_REPLAY_MOJOM_TRAITS_H_
#define CHROME_COMMON_RECORD_REPLAY_RECORD_REPLAY_MOJOM_TRAITS_H_

#include <string>

#include "chrome/common/record_replay/aliases.h"
#include "chrome/common/record_replay/record_replay.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<record_replay::mojom::DomNodeIdDataView,
                    record_replay::DomNodeId> {
  static int64_t id(const record_replay::DomNodeId& id) { return *id; }

  static bool Read(record_replay::mojom::DomNodeIdDataView data,
                   record_replay::DomNodeId* out);
};

template <>
struct StructTraits<record_replay::mojom::FieldValueDataView,
                    record_replay::FieldValue> {
  static const std::string& value(const record_replay::FieldValue& v) {
    return *v;
  }

  static bool Read(record_replay::mojom::FieldValueDataView data,
                   record_replay::FieldValue* out);
};

template <>
struct StructTraits<record_replay::mojom::SelectorDataView,
                    record_replay::Selector> {
  static const std::string& selector(const record_replay::Selector& s) {
    return *s;
  }

  static bool Read(record_replay::mojom::SelectorDataView data,
                   record_replay::Selector* out);
};

}  // namespace mojo

#endif  // CHROME_COMMON_RECORD_REPLAY_RECORD_REPLAY_MOJOM_TRAITS_H_
