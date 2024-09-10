// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_MOJOM_HISTORY_TYPES_MOJOM_TRAITS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_MOJOM_HISTORY_TYPES_MOJOM_TRAITS_H_

#include "base/containers/fixed_flat_map.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/sync_device_info/device_info.h"

namespace mojo {

template <>
struct EnumTraits<history::mojom::FormFactor, syncer::DeviceInfo::FormFactor> {
  static history::mojom::FormFactor ToMojom(
      syncer::DeviceInfo::FormFactor input) {
    static constexpr auto form_factor_map =
        base::MakeFixedFlatMap<syncer::DeviceInfo::FormFactor,
                               history::mojom::FormFactor>(
            {{syncer::DeviceInfo::FormFactor::kUnknown,
              history::mojom::FormFactor::kUnknown},
             {syncer::DeviceInfo::FormFactor::kDesktop,
              history::mojom::FormFactor::kDesktop},
             {syncer::DeviceInfo::FormFactor::kPhone,
              history::mojom::FormFactor::kPhone},
             {syncer::DeviceInfo::FormFactor::kTablet,
              history::mojom::FormFactor::kTablet},
             {syncer::DeviceInfo::FormFactor::kAutomotive,
              history::mojom::FormFactor::kAutomotive},
             {syncer::DeviceInfo::FormFactor::kWearable,
              history::mojom::FormFactor::kWearable},
             {syncer::DeviceInfo::FormFactor::kTv,
              history::mojom::FormFactor::kTv}});
    return form_factor_map.at(input);
  }

  static bool FromMojom(history::mojom::FormFactor input,
                        syncer::DeviceInfo::FormFactor* out) {
    static constexpr auto form_factor_map =
        base::MakeFixedFlatMap<history::mojom::FormFactor,
                               syncer::DeviceInfo::FormFactor>(
            {{history::mojom::FormFactor::kUnknown,
              syncer::DeviceInfo::FormFactor::kUnknown},
             {history::mojom::FormFactor::kDesktop,
              syncer::DeviceInfo::FormFactor::kDesktop},
             {history::mojom::FormFactor::kPhone,
              syncer::DeviceInfo::FormFactor::kPhone},
             {history::mojom::FormFactor::kTablet,
              syncer::DeviceInfo::FormFactor::kTablet},
             {history::mojom::FormFactor::kAutomotive,
              syncer::DeviceInfo::FormFactor::kAutomotive},
             {history::mojom::FormFactor::kWearable,
              syncer::DeviceInfo::FormFactor::kWearable},
             {history::mojom::FormFactor::kTv,
              syncer::DeviceInfo::FormFactor::kTv}});
    *out = form_factor_map.at(input);
    return true;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_MOJOM_HISTORY_TYPES_MOJOM_TRAITS_H_
