// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_page_handler.h"

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"

PerformancePageHandler::PerformancePageHandler(
    mojo::PendingReceiver<side_panel::mojom::PerformancePageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::PerformancePage> page,
    PerformanceSidePanelUI* performance_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      performance_ui_(performance_ui) {}

PerformancePageHandler::~PerformancePageHandler() = default;
