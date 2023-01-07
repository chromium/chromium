// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_MOJOM_SYSTEM_SIGNALS_MOJOM_TRAITS_WIN_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_MOJOM_SYSTEM_SIGNALS_MOJOM_TRAITS_WIN_H_

#include <string>

#include "components/device_signals/core/common/mojom/system_signals.mojom-shared.h"
#include "components/device_signals/core/common/win/win_types.h"

namespace mojo {

template <>
struct EnumTraits<device_signals::mojom::AntiVirusProductState,
                  device_signals::AvProductState> {
  static device_signals::mojom::AntiVirusProductState ToMojom(
      device_signals::AvProductState input);
  static bool FromMojom(device_signals::mojom::AntiVirusProductState input,
                        device_signals::AvProductState* output);
};

template <>
struct StructTraits<device_signals::mojom::AntiVirusSignalDataView,
                    device_signals::AvProduct> {
  static const std::string& display_name(
      const device_signals::AvProduct& input) {
    return input.display_name;
  }

  static const std::string& product_id(const device_signals::AvProduct& input) {
    return input.product_id;
  }

  static device_signals::AvProductState state(
      const device_signals::AvProduct& input) {
    return input.state;
  }

  static bool Read(device_signals::mojom::AntiVirusSignalDataView data,
                   device_signals::AvProduct* output);
};

template <>
struct StructTraits<device_signals::mojom::HotfixSignalDataView,
                    device_signals::InstalledHotfix> {
  static const std::string& hotfix_id(
      const device_signals::InstalledHotfix& input) {
    return input.hotfix_id;
  }

  static bool Read(device_signals::mojom::HotfixSignalDataView input,
                   device_signals::InstalledHotfix* output);
};

}  // namespace mojo

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_MOJOM_SYSTEM_SIGNALS_MOJOM_TRAITS_WIN_H_
