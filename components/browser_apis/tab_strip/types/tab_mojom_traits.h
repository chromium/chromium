// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_MOJOM_TRAITS_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom-shared.h"
#include "components/browser_apis/tab_strip/types/image_mojom_traits.h"
#include "components/browser_apis/tab_strip/types/node_id_mojom_traits.h"
#include "components/browser_apis/tab_strip/types/tab.h"
#include "components/tabs/public/mojom/tabs_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

using MojoTabView = tabs_api::mojom::TabDataView;
using NativeTab = tabs_api::Tab;

template <>
struct mojo::StructTraits<MojoTabView, NativeTab> {
  static const tabs_api::NodeId& id(const NativeTab& native);
  static std::string title(const NativeTab& native);
  static const GURL& url(const NativeTab& native);
  static const gfx::ImageSkia& favicon(const NativeTab& native);
  static const std::vector<tabs::TabAlert>& alert_states(
      const NativeTab& native);
  static tabs::TabNetworkState network_state(const NativeTab& native);
  static bool is_active(const NativeTab& native);
  static bool is_selected(const NativeTab& native);
  static bool is_blocked(const NativeTab& native);
  static base::TimeTicks last_active(const NativeTab& native);

  static bool Read(MojoTabView view, NativeTab* out);
};

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_MOJOM_TRAITS_H_
