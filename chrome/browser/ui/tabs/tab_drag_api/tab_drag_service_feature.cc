// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/tab_drag_service_feature.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"
#include "components/browser_apis/tab_drag/tab_drag_service_impl.h"

TabDragServiceFeature::TabDragServiceFeature(
    std::unique_ptr<tabs_api::TabDragWindowAdapter> window_adapter) {
  auto* manager = g_browser_process->GetFeatures()->tab_drag_session_manager();
  if (manager) {
    tab_drag_service_ = std::make_unique<tabs_api::TabDragServiceImpl>(
        manager, std::move(window_adapter));
  }
}

TabDragServiceFeature::~TabDragServiceFeature() = default;

void TabDragServiceFeature::AcceptDragService(
    mojo::PendingReceiver<tabs_api::mojom::TabDragService> client) {
  if (tab_drag_service_) {
    tab_drag_service_->Accept(std::move(client));
  }
}
