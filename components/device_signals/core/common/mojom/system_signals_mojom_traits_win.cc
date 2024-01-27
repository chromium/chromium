// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/mojom/system_signals_mojom_traits_win.h"

#include <optional>

#include "base/notreached.h"

namespace mojo {

// static
device_signals::mojom::AntiVirusProductState EnumTraits<
    device_signals::mojom::AntiVirusProductState,
    device_signals::AvProductState>::ToMojom(device_signals::AvProductState
                                                 input) {
  switch (input) {
    case device_signals::AvProductState::kOn:
      return device_signals::mojom::AntiVirusProductState::kOn;
    case device_signals::AvProductState::kOff:
      return device_signals::mojom::AntiVirusProductState::kOff;
    case device_signals::AvProductState::kSnoozed:
      return device_signals::mojom::AntiVirusProductState::kSnoozed;
    case device_signals::AvProductState::kExpired:
      return device_signals::mojom::AntiVirusProductState::kExpired;
  }
}

// static
bool EnumTraits<device_signals::mojom::AntiVirusProductState,
                device_signals::AvProductState>::
    FromMojom(device_signals::mojom::AntiVirusProductState input,
              device_signals::AvProductState* output) {
  std::optional<device_signals::AvProductState> parsed_state;
  switch (input) {
    case device_signals::mojom::AntiVirusProductState::kOn:
      parsed_state = device_signals::AvProductState::kOn;
      break;
    case device_signals::mojom::AntiVirusProductState::kOff:
      parsed_state = device_signals::AvProductState::kOff;
      break;
    case device_signals::mojom::AntiVirusProductState::kSnoozed:
      parsed_state = device_signals::AvProductState::kSnoozed;
      break;
    case device_signals::mojom::AntiVirusProductState::kExpired:
      parsed_state = device_signals::AvProductState::kExpired;
      break;
  }

  if (parsed_state.has_value()) {
    *output = parsed_state.value();
    return true;
  }
  return false;
}

// static
bool StructTraits<device_signals::mojom::AntiVirusSignalDataView,
                  device_signals::AvProduct>::
    Read(device_signals::mojom::AntiVirusSignalDataView data,
         device_signals::AvProduct* output) {
  std::string display_name;
  if (!data.ReadDisplayName(&display_name)) {
    return false;
  }
  output->display_name = display_name;

  std::string product_id;
  if (!data.ReadProductId(&product_id)) {
    return false;
  }
  output->product_id = product_id;

  device_signals::AvProductState state;
  if (!data.ReadState(&state)) {
    return false;
  }
  output->state = state;
  return true;
}

// static
bool StructTraits<device_signals::mojom::HotfixSignalDataView,
                  device_signals::InstalledHotfix>::
    Read(device_signals::mojom::HotfixSignalDataView data,
         device_signals::InstalledHotfix* output) {
  std::string hotfix_id;
  if (!data.ReadHotfixId(&hotfix_id)) {
    return false;
  }
  output->hotfix_id = hotfix_id;
  return true;
}

}  // namespace mojo
