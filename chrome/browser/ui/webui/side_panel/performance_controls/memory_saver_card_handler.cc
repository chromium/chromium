// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/performance_controls/memory_saver_card_handler.h"

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

MemorySaverCardHandler::MemorySaverCardHandler(
    mojo::PendingReceiver<side_panel::mojom::MemorySaverCardHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::MemorySaverCard> memory_saver_card,
    PerformanceSidePanelUI* performance_ui)
    : receiver_(this, std::move(receiver)),
      memory_saver_card_(std::move(memory_saver_card)),
      performance_ui_(performance_ui),
      profile_(Profile::FromWebUI(performance_ui_->web_ui())) {}

MemorySaverCardHandler::~MemorySaverCardHandler() = default;
