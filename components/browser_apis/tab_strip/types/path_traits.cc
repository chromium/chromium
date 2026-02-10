// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/path_traits.h"

bool mojo::StructTraits<MojoPathDataView, NativePath>::Read(
    MojoPathDataView view,
    NativePath* out) {
  std::vector<tabs_api::NodeId> components;
  if (!view.ReadComponents(&components)) {
    return false;
  }
  *out = tabs_api::Path(std::move(components));
  return true;
}
