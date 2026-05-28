// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/tab_drag_service_feature.h"

#include "components/browser_apis/tab_drag/tab_drag_service_impl.h"

TabDragServiceFeature::TabDragServiceFeature()
    : tab_drag_service_(std::make_unique<tabs_api::TabDragServiceImpl>()) {}

TabDragServiceFeature::~TabDragServiceFeature() = default;

void TabDragServiceFeature::AcceptDragService(
    mojo::PendingReceiver<tabs_api::mojom::TabDragService> client) {
  tab_drag_service_->Accept(std::move(client));
}
