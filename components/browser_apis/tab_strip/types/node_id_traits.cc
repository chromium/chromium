// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/node_id_traits.h"

MojoNodeIdType mojo::EnumTraits<MojoNodeIdType, NativeNodeType>::ToMojom(
    NativeNodeType input) {
  switch (input) {
    case NativeNodeType::kContent:
      return MojoNodeIdType::kContent;
    case NativeNodeType::kCollection:
      return MojoNodeIdType::kCollection;
    case NativeNodeType::kInvalid:
      return MojoNodeIdType::kUnknown;
  }

  NOTREACHED();
}

bool mojo::EnumTraits<MojoNodeIdType, NativeNodeType>::FromMojom(
    MojoNodeIdType in,
    NativeNodeType* out) {
  switch (in) {
    case MojoNodeIdType::kContent:
      *out = NativeNodeType::kContent;
      return true;
    case MojoNodeIdType::kCollection:
      *out = NativeNodeType::kCollection;
      return true;
    case MojoNodeIdType::kUnknown:
      *out = NativeNodeType::kInvalid;
      return true;
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
