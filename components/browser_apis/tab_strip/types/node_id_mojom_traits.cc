// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/node_id_mojom_traits.h"

MojoNodeIdType mojo::EnumTraits<MojoNodeIdType, NativeNodeType>::ToMojom(
    NativeNodeType input) {
  switch (input) {
    case NativeNodeType::kWindow:
      return MojoNodeIdType::kWindow;
    case NativeNodeType::kContent:
      return MojoNodeIdType::kContent;
    case NativeNodeType::kCollection:
      return MojoNodeIdType::kCollection;
    case NativeNodeType::kInvalid:
      return MojoNodeIdType::kUnknown;
  }

  NOTREACHED();
}

NativeNodeType mojo::EnumTraits<MojoNodeIdType, NativeNodeType>::FromMojom(
    MojoNodeIdType in) {
  switch (in) {
    case MojoNodeIdType::kWindow:
      return NativeNodeType::kWindow;
    case MojoNodeIdType::kContent:
      return NativeNodeType::kContent;
    case MojoNodeIdType::kCollection:
      return NativeNodeType::kCollection;
    case MojoNodeIdType::kUnknown:
      return NativeNodeType::kInvalid;
  }

  NOTREACHED();
}

std::string_view mojo::StructTraits<MojoNodeIdView, NativeNodeId>::id(
    const NativeNodeId& native) {
  return native.Id();
}

NativeNodeType mojo::StructTraits<MojoNodeIdView, NativeNodeId>::type(
    const NativeNodeId& native) {
  return native.Type();
}

bool mojo::StructTraits<MojoNodeIdView, NativeNodeId>::Read(MojoNodeIdView view,
                                                            NativeNodeId* out) {
  std::string id;
  if (!view.ReadId(&id)) {
    return false;
  }

  NativeNodeType type;
  if (!view.ReadType(&type)) {
    return false;
  }
  *out = NativeNodeId(type, id);
  return true;
}
