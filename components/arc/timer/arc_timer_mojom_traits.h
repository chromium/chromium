// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TIMER_ARC_TIMER_MOJOM_TRAITS_H_
#define COMPONENTS_ARC_TIMER_ARC_TIMER_MOJOM_TRAITS_H_

#include <time.h>

#include "components/arc/mojom/timer.mojom.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::ClockId, clockid_t> {
  static arc::mojom::ClockId ToMojom(clockid_t clock_id);
  static bool FromMojom(arc::mojom::ClockId input, clockid_t* output);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_TIMER_ARC_TIMER_MOJOM_TRAITS_H_
