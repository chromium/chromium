// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_window_adapter_impl.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

TabDragWindowAdapterImpl::TabDragWindowAdapterImpl(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window) {}

TabDragWindowAdapterImpl::~TabDragWindowAdapterImpl() = default;

gfx::Point TabDragWindowAdapterImpl::ConvertScreenPointToLocal(
    const gfx::Point& screen_point) const {
  // TODO(crbug.com/501070453): Implement this once a client is registered
  NOTIMPLEMENTED();
  return screen_point;
}

base::WeakPtr<tabs_api::TabDragWindowAdapter>
TabDragWindowAdapterImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
