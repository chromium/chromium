// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/search/magic_boost_notice_page_handler_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/webui/ash/settings/pages/search/mojom/magic_boost_handler.mojom.h"

namespace ash::settings {

MagicBoostNoticePageHandlerFactory::MagicBoostNoticePageHandlerFactory(
    Profile* profile,
    mojo::PendingReceiver<magic_boost_handler::mojom::PageHandlerFactory>
        receiver)
    : profile_(profile) {
  page_factory_receiver_.Bind(std::move(receiver));
}

MagicBoostNoticePageHandlerFactory::~MagicBoostNoticePageHandlerFactory() =
    default;

void MagicBoostNoticePageHandlerFactory::CreatePageHandler(
    mojo::PendingReceiver<magic_boost_handler::mojom::PageHandler> receiver) {
  DCHECK(!page_handler_);

  page_handler_ =
      std::make_unique<MagicBoostNoticeHandler>(std::move(receiver), profile_);
}

}  // namespace ash::settings
