// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_ENUM_TRAITS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_ENUM_TRAITS_H_

#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

using MojoTabNetworkState = tabs_api::mojom::TabNetworkState;
using NativeTabNetworkState = enum TabNetworkState;

// TabNetworkState Enum mapping.
template <>
struct mojo::EnumTraits<MojoTabNetworkState, NativeTabNetworkState> {
  static MojoTabNetworkState ToMojom(NativeTabNetworkState input);
  static bool FromMojom(MojoTabNetworkState in, NativeTabNetworkState* out);
};

using MojoTabAlertState = tabs_api::mojom::TabAlertState;
using NativeTabAlertState = enum TabAlertState;

// TabAlertState Enum mapping.
template <>
struct mojo::EnumTraits<MojoTabAlertState, NativeTabAlertState> {
  static MojoTabAlertState ToMojom(NativeTabAlertState input);
  static bool FromMojom(MojoTabAlertState in, NativeTabAlertState* out);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_ENUM_TRAITS_H_
