// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_desktop_injector.h"

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_factory.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry_impl.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_event_router.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"

namespace tabs_api {

TabDragSessionDesktopInjector::TabDragSessionDesktopInjector()
    : window_registry_(std::make_unique<TabDragWindowRegistry>()),
      adapter_(TabDragSessionInputAdapterFactory::Create()),
      registry_(std::make_unique<DropTargetRegistryImpl>()),
      event_router_(std::make_unique<TabDragEventRouter>(*registry_)) {}

TabDragSessionDesktopInjector::~TabDragSessionDesktopInjector() = default;

TabDragWindowRegistry* TabDragSessionDesktopInjector::GetWindowRegistry() {
  return window_registry_.get();
}

TabDragSessionInputAdapter& TabDragSessionDesktopInjector::GetInputAdapter() {
  return *adapter_;
}

TabDragSessionListener& TabDragSessionDesktopInjector::GetSessionListener() {
  return *event_router_;
}

DropTargetRegistry& TabDragSessionDesktopInjector::GetDropTargetRegistry() {
  return *registry_;
}

}  // namespace tabs_api
