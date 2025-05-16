// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TABS_TABS_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_TABS_TABS_MOJOM_TRAITS_H_

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/webui/tabs/tabs.mojom.h"

// TODO(crbug.com/403572608) Autogenerate traits for simple enum cases.
namespace mojo {

using MojoTabNetworkState = tabs::mojom::TabNetworkState;
using NativeTabNetworkState = enum TabNetworkState;

// TabNetworkState Enum mapping.
template <>
struct EnumTraits<MojoTabNetworkState, NativeTabNetworkState> {
  static MojoTabNetworkState ToMojom(NativeTabNetworkState input);
  static bool FromMojom(MojoTabNetworkState in, NativeTabNetworkState* out);
};

using MojoTabAlertState = tabs::mojom::TabAlertState;
using NativeTabAlertState = enum tabs::TabAlert;

// TabAlertState Enum mapping.
template <>
struct EnumTraits<MojoTabAlertState, NativeTabAlertState> {
  static MojoTabAlertState ToMojom(NativeTabAlertState input);
  static bool FromMojom(MojoTabAlertState in, NativeTabAlertState* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_TABS_TABS_MOJOM_TRAITS_H_
