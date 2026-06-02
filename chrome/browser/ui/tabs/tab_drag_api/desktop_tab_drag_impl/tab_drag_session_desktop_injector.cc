// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_desktop_injector.h"

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_impl.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"

namespace tabs_api {

TabDragSessionDesktopInjector::TabDragSessionDesktopInjector()
    : adapter_(std::make_unique<TabDragSessionInputAdapterImpl>()) {}

TabDragSessionDesktopInjector::~TabDragSessionDesktopInjector() = default;

TabDragSessionInputAdapter&
TabDragSessionDesktopInjector::tab_drag_session_input_adapter() {
  return *adapter_;
}

}  // namespace tabs_api
