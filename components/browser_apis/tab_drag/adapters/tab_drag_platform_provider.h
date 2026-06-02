// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_PLATFORM_PROVIDER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_PLATFORM_PROVIDER_H_

namespace tabs_api {

class TabDragSessionInputAdapter;

// Platform-agnostic integration provider for tab dragging.
// Implemented under the browser UI window context.
class TabDragPlatformProvider {
 public:
  virtual ~TabDragPlatformProvider() = default;

  // Retrieves the platform-specific input capturing delegate.
  virtual TabDragSessionInputAdapter& tab_drag_session_input_adapter() = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_PLATFORM_PROVIDER_H_
