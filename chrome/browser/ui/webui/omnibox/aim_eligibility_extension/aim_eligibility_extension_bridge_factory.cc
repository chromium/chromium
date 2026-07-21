// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge_factory.h"

#include <memory>

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "extensions/browser/extension_mojo_binder_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace {

class AimEligibilityExtensionBinderProvider
    : public extensions::ExtensionMojoBinderProvider {
 public:
  AimEligibilityExtensionBinderProvider() = default;
  ~AimEligibilityExtensionBinderProvider() override = default;

  extensions::ExtensionId GetExtensionId() const override {
    return extension_misc::kAimEligibilityExtensionId;
  }

  void PopulateFrameBinders(
      extensions::ExtensionBinderMap<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const extensions::Extension* extension) override {
    binder_map.Add<aim_eligibility::mojom::PageHandlerFactory>(
        base::BindRepeating(
            [](content::RenderFrameHost* frame_host,
               mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
                   receiver) {
              auto* bridge = AimEligibilityExtensionBridge::Get(
                  Profile::FromBrowserContext(frame_host->GetBrowserContext()));
              if (bridge) {
                bridge->BindFactoryReceiver(std::move(receiver));
              }
            }));

    binder_map.Add<color_change_listener::mojom::PageHandler>(
        base::BindRepeating(
            [](content::RenderFrameHost* frame_host,
               mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
                   receiver) {
              auto* handler =
                  ui::ColorChangeHandler::GetOrCreateForCurrentDocument(
                      frame_host);
              handler->Bind(std::move(receiver),
                            /*allow_non_webui=*/true);
            }));
  }

  void PopulateServiceWorkerBinders(
      extensions::ExtensionBinderMap<
          const content::ServiceWorkerVersionBaseInfo&>& binder_map,
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) override {
    binder_map.Add<aim_eligibility::mojom::PageHandlerFactory>(
        base::BindRepeating(
            [](content::BrowserContext* browser_context,
               const content::ServiceWorkerVersionBaseInfo&,
               mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
                   receiver) {
              auto* bridge = AimEligibilityExtensionBridge::Get(
                  Profile::FromBrowserContext(browser_context));
              if (bridge) {
                bridge->BindFactoryReceiver(std::move(receiver));
              }
            },
            browser_context));
  }
};

}  // namespace

// static
AimEligibilityExtensionBridge*
AimEligibilityExtensionBridgeFactory::GetForProfile(Profile* profile) {
  return static_cast<AimEligibilityExtensionBridge*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AimEligibilityExtensionBridgeFactory*
AimEligibilityExtensionBridgeFactory::GetInstance() {
  static base::NoDestructor<AimEligibilityExtensionBridgeFactory> instance;
  return instance.get();
}

AimEligibilityExtensionBridgeFactory::AimEligibilityExtensionBridgeFactory()
    : ProfileKeyedServiceFactory(
          "AimEligibilityExtensionBridge",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(AimEligibilityServiceFactory::GetInstance());

  extensions::ExtensionMojoBinderRegistry::GetInstance()->RegisterProvider(
      std::make_unique<AimEligibilityExtensionBinderProvider>());
}

AimEligibilityExtensionBridgeFactory::~AimEligibilityExtensionBridgeFactory() =
    default;

std::unique_ptr<KeyedService>
AimEligibilityExtensionBridgeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AimEligibilityExtensionBridge>(
      Profile::FromBrowserContext(context));
}

bool AimEligibilityExtensionBridgeFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
