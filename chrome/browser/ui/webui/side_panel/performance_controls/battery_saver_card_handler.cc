// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/performance_controls/battery_saver_card_handler.h"

BatterySaverCardHandler::BatterySaverCardHandler(
    mojo::PendingReceiver<side_panel::mojom::BatterySaverCardHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::BatterySaverCard> battery_saver_card)
    : receiver_(this, std::move(receiver)),
      battery_saver_card_(std::move(battery_saver_card)) {}

BatterySaverCardHandler::~BatterySaverCardHandler() = default;
