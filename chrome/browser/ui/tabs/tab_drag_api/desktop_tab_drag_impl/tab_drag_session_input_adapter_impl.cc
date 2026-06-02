// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_impl.h"

namespace tabs_api {

TabDragSessionInputAdapterImpl::TabDragSessionInputAdapterImpl() = default;
TabDragSessionInputAdapterImpl::~TabDragSessionInputAdapterImpl() = default;

void TabDragSessionInputAdapterImpl::StartInputCapture(
    const std::vector<tabs_api::NodeId>& source_tab_ids) {}

void TabDragSessionInputAdapterImpl::ReleaseInputCapture() {}

}  // namespace tabs_api
