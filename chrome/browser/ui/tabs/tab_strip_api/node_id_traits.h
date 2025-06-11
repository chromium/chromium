// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_NODE_ID_TRAITS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_NODE_ID_TRAITS_H_

#include "chrome/browser/ui/tabs/tab_strip_api/node_id.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

using MojoNodeIdType = tabs_api::mojom::NodeId_Type;
using NativeNodeType = enum tabs_api::NodeId::Type;

// Tab type Enum mapping.
template <>
struct mojo::EnumTraits<MojoNodeIdType, NativeNodeType> {
  static MojoNodeIdType ToMojom(NativeNodeType input);
  static bool FromMojom(MojoNodeIdType in, NativeNodeType* out);
};

using MojoNodeIdView = tabs_api::mojom::NodeIdDataView;
using NativeNodeId = tabs_api::NodeId;

// TabId Struct mapping.
template <>
struct mojo::StructTraits<MojoNodeIdView, NativeNodeId> {
  // Field getters:
  static std::string_view id(const NativeNodeId& native);
  static NativeNodeType type(const NativeNodeId& native);

  // Decoder:
  static bool Read(MojoNodeIdView view, NativeNodeId* out);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_NODE_ID_TRAITS_H_
