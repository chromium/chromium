// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_TOOLBAR_ICON_HANDLE_MOJOM_TRAITS_H_
#define COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_TOOLBAR_ICON_HANDLE_MOJOM_TRAITS_H_

#include "components/browser_apis/ui_controllers/toolbar/icon_handle.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"

namespace mojo {

template <>
class StructTraits<toolbar_ui_api::mojom::IconHandleDataView,
                   toolbar_ui_api::IconHandle> {
 public:
  static uint64_t handle_id(const toolbar_ui_api::IconHandle& handle) {
    return handle.HandleId().value();
  }

  static bool Read(toolbar_ui_api::mojom::IconHandleDataView data,
                   toolbar_ui_api::IconHandle* out);
};

}  // namespace mojo

#endif  // COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_TOOLBAR_ICON_HANDLE_MOJOM_TRAITS_H_
