// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_NODE_ID_TRAITS_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_NODE_ID_TRAITS_H_

#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
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

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_NODE_ID_TRAITS_H_
