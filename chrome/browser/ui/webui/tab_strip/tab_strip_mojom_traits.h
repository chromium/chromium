// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_MOJOM_TRAITS_H_

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip.mojom.h"

namespace mojo {

template <>
struct EnumTraits<tab_strip::mojom::TabNetworkState, TabNetworkState> {
  static tab_strip::mojom::TabNetworkState ToMojom(TabNetworkState input) {
    static constexpr auto network_state_map = base::MakeFixedFlatMap<
        TabNetworkState, tab_strip::mojom::TabNetworkState>(
        {{TabNetworkState::kNone, tab_strip::mojom::TabNetworkState::kNone},
         {TabNetworkState::kWaiting,
          tab_strip::mojom::TabNetworkState::kWaiting},
         {TabNetworkState::kLoading,
          tab_strip::mojom::TabNetworkState::kLoading},
         {TabNetworkState::kError, tab_strip::mojom::TabNetworkState::kError}});
    return network_state_map.at(input);
  }

  static bool FromMojom(tab_strip::mojom::TabNetworkState input,
                        TabNetworkState* out) {
    static constexpr auto network_state_map = base::MakeFixedFlatMap<
        tab_strip::mojom::TabNetworkState, TabNetworkState>(
        {{tab_strip::mojom::TabNetworkState::kNone, TabNetworkState::kNone},
         {tab_strip::mojom::TabNetworkState::kWaiting,
          TabNetworkState::kWaiting},
         {tab_strip::mojom::TabNetworkState::kLoading,
          TabNetworkState::kLoading},
         {tab_strip::mojom::TabNetworkState::kError, TabNetworkState::kError}});
    *out = network_state_map.at(input);
    return true;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_MOJOM_TRAITS_H_
