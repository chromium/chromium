// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_PATH_TRAITS_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_PATH_TRAITS_H_

#include <vector>

#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom-shared.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/browser_apis/tab_strip/types/node_id_traits.h"
#include "components/browser_apis/tab_strip/types/path.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

using MojoPathDataView = tabs_api::mojom::PathDataView;
using NativePath = tabs_api::Path;

template <>
struct mojo::StructTraits<MojoPathDataView, NativePath> {
  static const std::vector<tabs_api::NodeId>& components(
      const NativePath& path) {
    return path.components();
  }

  static bool Read(MojoPathDataView view, NativePath* out);
};

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_PATH_TRAITS_H_
