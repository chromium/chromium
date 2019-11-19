// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MOJOM_SYNCER_MOJOM_TRAITS_H_
#define COMPONENTS_SYNC_MOJOM_SYNCER_MOJOM_TRAITS_H_

#include <string>

#include "components/sync/model/string_ordinal.h"
#include "components/sync/mojom/syncer.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<syncer::mojom::StringOrdinalDataView,
                    syncer::StringOrdinal> {
  static base::span<const uint8_t> bytes(const syncer::StringOrdinal& ordinal) {
    return base::make_span(
        reinterpret_cast<const uint8_t*>(ordinal.bytes_.data()),
        ordinal.bytes_.length());
  }

  static bool Read(syncer::mojom::StringOrdinalDataView data,
                   syncer::StringOrdinal* out) {
    mojo::ArrayDataView<uint8_t> bytes;
    data.GetBytesDataView(&bytes);
    *out = syncer::StringOrdinal(
        std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    return true;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_SYNC_MOJOM_SYNCER_MOJOM_TRAITS_H_
