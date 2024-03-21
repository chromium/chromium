// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_page_handler.h"

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::extended_updates {

ExtendedUpdatesPageHandler::ExtendedUpdatesPageHandler(
    mojo::PendingRemote<ash::extended_updates::mojom::Page> page,
    mojo::PendingReceiver<ash::extended_updates::mojom::PageHandler> receiver)
    : page_(std::move(page)), receiver_(this, std::move(receiver)) {}

ExtendedUpdatesPageHandler::~ExtendedUpdatesPageHandler() = default;

void ExtendedUpdatesPageHandler::OptInToExtendedUpdates(
    OptInToExtendedUpdatesCallback callback) {
  std::move(callback).Run(true);
  return;
}

}  // namespace ash::extended_updates
