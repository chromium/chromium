// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_TIMER_ARC_TIMER_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_TIMER_ARC_TIMER_MOJOM_TRAITS_H_

#include <time.h>

#include "chromeos/ash/experiences/arc/mojom/timer.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::ClockId, clockid_t> {
  static arc::mojom::ClockId ToMojom(clockid_t clock_id);
  static bool FromMojom(arc::mojom::ClockId input, clockid_t* output);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_TIMER_ARC_TIMER_MOJOM_TRAITS_H_
