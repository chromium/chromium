// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/files/google_drive_page_handler_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/google_drive_handler.mojom.h"

namespace ash::settings {

GoogleDrivePageHandlerFactory::GoogleDrivePageHandlerFactory(
    Profile* profile,
    mojo::PendingReceiver<google_drive::mojom::PageHandlerFactory> receiver)
    : profile_(profile) {
  page_factory_receiver_.Bind(std::move(receiver));
}

GoogleDrivePageHandlerFactory::~GoogleDrivePageHandlerFactory() = default;

void GoogleDrivePageHandlerFactory::CreatePageHandler(
    mojo::PendingRemote<google_drive::mojom::Page> page,
    mojo::PendingReceiver<google_drive::mojom::PageHandler> receiver) {
  DCHECK(page);
  DCHECK(!page_handler_);

  page_handler_ = std::make_unique<GoogleDrivePageHandler>(
      std::move(receiver), std::move(page), profile_);
}

}  // namespace ash::settings
