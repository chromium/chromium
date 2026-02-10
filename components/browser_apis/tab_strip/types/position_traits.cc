// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/position_traits.h"

const tabs_api::Path&
mojo::StructTraits<MojoPositionView, NativePosition>::path(
    const NativePosition& native) {
  return native.path();
}

uint32_t mojo::StructTraits<MojoPositionView, NativePosition>::index(
    const NativePosition& native) {
  return native.index();
}

bool mojo::StructTraits<MojoPositionView, NativePosition>::Read(
    MojoPositionView view,
    NativePosition* out) {
  tabs_api::Path path;
  if (!view.ReadPath(&path)) {
    return false;
  }

  *out = tabs_api::Position(view.index(), std::move(path));
  return true;
}
