// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_service_worker_page_handler_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility_page_handler.h"

AimEligibilityExtensionServiceWorkerPageHandlerFactory::
    AimEligibilityExtensionServiceWorkerPageHandlerFactory(Profile& profile)
    : profile_(profile) {}

AimEligibilityExtensionServiceWorkerPageHandlerFactory::
    ~AimEligibilityExtensionServiceWorkerPageHandlerFactory() = default;

void AimEligibilityExtensionServiceWorkerPageHandlerFactory::Bind(
    mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AimEligibilityExtensionServiceWorkerPageHandlerFactory::Clear() {
  receivers_.Clear();
  page_handlers_.clear();
}

void AimEligibilityExtensionServiceWorkerPageHandlerFactory::CreatePageHandler(
    mojo::PendingRemote<aim_eligibility::mojom::Page> page,
    mojo::PendingReceiver<aim_eligibility::mojom::PageHandler> handler) {
  auto handler_instance = std::make_unique<AimEligibilityPageHandler>(
      &profile_.get(), std::move(handler), std::move(page));
  AimEligibilityPageHandler* raw_handler = handler_instance.get();
  raw_handler->set_disconnect_handler(base::BindOnce(
      [](AimEligibilityExtensionServiceWorkerPageHandlerFactory* factory,
         AimEligibilityPageHandler* handler_to_erase) {
        factory->page_handlers_.erase(handler_to_erase);
      },
      base::Unretained(this), raw_handler));
  page_handlers_.insert(std::move(handler_instance));
}
