// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/mojom/arc_gfx_mojom_traits.h"

namespace mojo {

bool StructTraits<arc::mojom::RectDataView, gfx::Rect>::Read(
    arc::mojom::RectDataView data,
    gfx::Rect* out) {
  if (data.right() < data.left() || data.bottom() < data.top())
    return false;

  out->SetRect(data.left(), data.top(), data.right() - data.left(),
               data.bottom() - data.top());
  return true;
}

bool StructTraits<arc::mojom::RangeDataView, gfx::Range>::Read(
    arc::mojom::RangeDataView data,
    gfx::Range* out) {
  out->set_start(data.start());
  out->set_end(data.end());
  return true;
}

}  // namespace mojo
