// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_binder_provider.h"

#include <memory>

#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility.mojom.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_frame_page_handler_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"
#include "extensions/common/constants.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace {

void BindFactoryReceiverForFrame(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
        receiver) {
  AimEligibilityExtensionFramePageHandlerFactory::GetOrCreateForCurrentDocument(
      frame_host)
      ->Bind(std::move(receiver));
}

void BindFactoryReceiverForWorker(
    content::BrowserContext* context,
    const content::ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
        receiver) {
  auto* bridge =
      AimEligibilityExtensionBridge::Get(Profile::FromBrowserContext(context));
  CHECK(bridge);
  bridge->service_worker_page_handler_factory().Bind(std::move(receiver));
}

}  // namespace

// static
void AimEligibilityExtensionBinderProvider::Register(
    content::BrowserContext* browser_context) {
  extensions::ExtensionMojoBinderRegistryFactory::GetOrCreateForBrowserContext(
      browser_context)
      ->RegisterProvider(
          base::PassKey<AimEligibilityExtensionBinderProvider>(),
          std::make_unique<AimEligibilityExtensionBinderProvider>());
}

AimEligibilityExtensionBinderProvider::AimEligibilityExtensionBinderProvider()
    : ExtensionMojoBinderProvider(extension_misc::kAimEligibilityExtensionId) {}

AimEligibilityExtensionBinderProvider::
    ~AimEligibilityExtensionBinderProvider() = default;

bool AimEligibilityExtensionBinderProvider::IsMojoJsEnabledForFrame() const {
  return true;
}

bool AimEligibilityExtensionBinderProvider::IsMojoJsEnabledForServiceWorker()
    const {
  return true;
}

void AimEligibilityExtensionBinderProvider::PopulateFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
    content::RenderFrameHost* render_frame_host,
    const extensions::Extension& extension) {
  binder_map.Add<aim_eligibility::mojom::PageHandlerFactory>(
      base::BindRepeating(&BindFactoryReceiverForFrame));

  binder_map.Add<color_change_listener::mojom::PageHandler>(base::BindRepeating(
      [](content::RenderFrameHost* frame_host,
         mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
             receiver) {
        auto* handler =
            ui::ColorChangeHandler::GetOrCreateForCurrentDocument(frame_host);
        handler->Bind(std::move(receiver),
                      /*allow_non_webui=*/true);
      }));
}

void AimEligibilityExtensionBinderProvider::PopulateServiceWorkerBinders(
    mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>&
        binder_map,
    content::BrowserContext* browser_context,
    const extensions::Extension& extension) {
  binder_map.Add<aim_eligibility::mojom::PageHandlerFactory>(
      base::BindRepeating(&BindFactoryReceiverForWorker, browser_context));
}
