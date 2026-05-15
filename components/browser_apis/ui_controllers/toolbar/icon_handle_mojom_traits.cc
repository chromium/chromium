// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/ui_controllers/toolbar/icon_handle_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<toolbar_ui_api::mojom::IconHandleDataView,
                  toolbar_ui_api::IconHandle>::
    Read(toolbar_ui_api::mojom::IconHandleDataView data,
         toolbar_ui_api::IconHandle* out) {
  // In production, handles always go C++ -> .JS, but some tests want to
  // look at types that include them. Unfortunately, it's impossible to
  // usefully construct an IconHandle w/o a context that interprets the
  // handle rep, so this just always returns a null handle.
  *out = toolbar_ui_api::IconHandle();
  return true;
}

}  // namespace mojo
