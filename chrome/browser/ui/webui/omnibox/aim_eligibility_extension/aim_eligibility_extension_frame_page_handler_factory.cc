// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_frame_page_handler_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility_page_handler.h"
#include "content/public/browser/render_frame_host.h"

DOCUMENT_USER_DATA_KEY_IMPL(AimEligibilityExtensionFramePageHandlerFactory);

AimEligibilityExtensionFramePageHandlerFactory::
    AimEligibilityExtensionFramePageHandlerFactory(
        content::RenderFrameHost* rfh)
    : content::DocumentUserData<AimEligibilityExtensionFramePageHandlerFactory>(
          rfh) {}

AimEligibilityExtensionFramePageHandlerFactory::
    ~AimEligibilityExtensionFramePageHandlerFactory() = default;

void AimEligibilityExtensionFramePageHandlerFactory::Bind(
    mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void AimEligibilityExtensionFramePageHandlerFactory::CreatePageHandler(
    mojo::PendingRemote<aim_eligibility::mojom::Page> page,
    mojo::PendingReceiver<aim_eligibility::mojom::PageHandler> handler) {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  page_handler_ = std::make_unique<AimEligibilityPageHandler>(
      profile, std::move(handler), std::move(page));
}
