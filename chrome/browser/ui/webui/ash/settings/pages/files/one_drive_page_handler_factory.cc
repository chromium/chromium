// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/files/one_drive_page_handler_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/one_drive_handler.mojom.h"

namespace ash::settings {

OneDrivePageHandlerFactory::OneDrivePageHandlerFactory(
    Profile* profile,
    mojo::PendingReceiver<one_drive::mojom::PageHandlerFactory> receiver)
    : profile_(profile) {
  page_factory_receiver_.Bind(std::move(receiver));
}

OneDrivePageHandlerFactory::~OneDrivePageHandlerFactory() = default;

void OneDrivePageHandlerFactory::CreatePageHandler(
    mojo::PendingRemote<one_drive::mojom::Page> page,
    mojo::PendingReceiver<one_drive::mojom::PageHandler> receiver) {
  DCHECK(page);
  DCHECK(!page_handler_);

  page_handler_ = std::make_unique<OneDrivePageHandler>(
      std::move(receiver), std::move(page), profile_);
}

}  // namespace ash::settings
