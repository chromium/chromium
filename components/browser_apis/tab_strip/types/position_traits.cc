// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/position_traits.h"

const std::optional<tabs_api::NodeId>&
mojo::StructTraits<MojoPositionView, NativePosition>::parent_id(
    const NativePosition& native) {
  return native.parent_id();
}

uint32_t mojo::StructTraits<MojoPositionView, NativePosition>::index(
    const NativePosition& native) {
  return native.index();
}

bool mojo::StructTraits<MojoPositionView, NativePosition>::Read(
    MojoPositionView view,
    NativePosition* out) {
  std::optional<tabs_api::NodeId> parent_id;
  if (!view.ReadParentId(&parent_id)) {
    return false;
  }

  *out = tabs_api::Position(view.index(), std::move(parent_id));
  return true;
}
