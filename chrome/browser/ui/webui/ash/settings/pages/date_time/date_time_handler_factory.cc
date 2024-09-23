// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/date_time/date_time_handler_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/webui/ash/settings/pages/date_time/mojom/date_time_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/new_date_time_handler.h"

namespace ash::settings {

DateTimeHandlerFactory::DateTimeHandlerFactory(
    content::WebUI* web_ui,
    Profile* profile,
    mojo::PendingReceiver<date_time::mojom::PageHandlerFactory> receiver)
    : web_ui_(web_ui), profile_(profile) {
  page_factory_receiver_.Bind(std::move(receiver));
}

DateTimeHandlerFactory::~DateTimeHandlerFactory() = default;

void DateTimeHandlerFactory::CreatePageHandler(
    mojo::PendingRemote<date_time::mojom::Page> page,
    mojo::PendingReceiver<date_time::mojom::PageHandler> receiver) {
  DCHECK(page);
  DCHECK(!page_handler_);

  page_handler_ = std::make_unique<NewDateTimeHandler>(
      std::move(receiver), std::move(page), web_ui_, profile_);
}

}  // namespace ash::settings
