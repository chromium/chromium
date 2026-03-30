// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_MOJOM_TABS_MOJOM_TRAITS_H_
#define COMPONENTS_TABS_PUBLIC_MOJOM_TABS_MOJOM_TRAITS_H_

#include "components/tabs/public/mojom/tabs.mojom.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_network_state.h"

// TODO(crbug.com/403572608) Autogenerate traits for simple enum cases.
namespace mojo {

using MojoTabNetworkState = tabs::mojom::TabNetworkState;
using NativeTabNetworkState = enum tabs::TabNetworkState;

// TabNetworkState Enum mapping.
template <>
struct EnumTraits<MojoTabNetworkState, NativeTabNetworkState> {
  static MojoTabNetworkState ToMojom(NativeTabNetworkState input);
  static NativeTabNetworkState FromMojom(MojoTabNetworkState in);
};

using MojoTabAlertState = tabs::mojom::TabAlertState;
using NativeTabAlertState = enum tabs::TabAlert;

// TabAlertState Enum mapping.
template <>
struct EnumTraits<MojoTabAlertState, NativeTabAlertState> {
  static MojoTabAlertState ToMojom(NativeTabAlertState input);
  static NativeTabAlertState FromMojom(MojoTabAlertState in);
};

}  // namespace mojo

#endif  // COMPONENTS_TABS_PUBLIC_MOJOM_TABS_MOJOM_TRAITS_H_
