// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_ID_TRAITS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_ID_TRAITS_H_

#include "chrome/browser/ui/tabs/tab_strip_api/tab_id.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

using MojoTabType = tabs_api::mojom::TabId_Type;
using NativeTabType = enum tabs_api::TabId::Type;

// Tab type Enum mapping.
template <>
struct mojo::EnumTraits<MojoTabType, NativeTabType> {
  static MojoTabType ToMojom(NativeTabType input);
  static bool FromMojom(MojoTabType in, NativeTabType* out);
};

using MojoTabIdView = tabs_api::mojom::TabIdDataView;
using NativeTabId = tabs_api::TabId;

// TabId Struct mapping.
template <>
struct mojo::StructTraits<MojoTabIdView, NativeTabId> {
  // Field getters:
  static std::string_view id(const NativeTabId& native);
  static NativeTabType type(const NativeTabId& native);

  // Decoder:
  static bool Read(MojoTabIdView view, NativeTabId* out);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_ID_TRAITS_H_
