// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility/extension/aim_eligibility_extension_bridge.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility_page_handler.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility/extension/aim_eligibility_extension_bridge_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"

namespace extensions {

AimEligibilityExtensionBridge::AimEligibilityExtensionBridge(Profile* profile)
    : profile_(*profile) {
  auto* resource_manager =
      ExtensionsBrowserClient::Get()->GetComponentExtensionResourceManager();
  if (resource_manager) {
    load_time_data_subscription_ =
        resource_manager->RegisterTemplateDataProvider(
            extension_misc::kAimEligibilityExtensionId, &profile_.get(),
            base::BindRepeating(&AimEligibilityExtensionBridge::GetLoadTimeData,
                                base::Unretained(this)));
  }
}

AimEligibilityExtensionBridge::~AimEligibilityExtensionBridge() = default;

base::DictValue AimEligibilityExtensionBridge::GetLoadTimeData() {
  base::DictValue dict;
  // The following keys (`aimEligibilityTitle`, `showAimEligibilityFooter`) are
  // for illustration and parity purposes with the WebUI version.
  dict.Set("aimEligibilityTitle", "AIM Eligibility Diagnostic");
  dict.Set("showAimEligibilityFooter", true);
  return dict;
}

void AimEligibilityExtensionBridge::Shutdown() {
  receivers_.Clear();
  page_handlers_.clear();
}

// static
AimEligibilityExtensionBridge* AimEligibilityExtensionBridge::Get(
    Profile* profile) {
  return AimEligibilityExtensionBridgeFactory::GetForProfile(profile);
}

void AimEligibilityExtensionBridge::BindFactoryReceiver(
    mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AimEligibilityExtensionBridge::CreatePageHandler(
    mojo::PendingRemote<aim_eligibility::mojom::Page> page,
    mojo::PendingReceiver<aim_eligibility::mojom::PageHandler> handler) {
  auto handler_instance = std::make_unique<AimEligibilityPageHandler>(
      &profile_.get(), std::move(handler), std::move(page));
  AimEligibilityPageHandler* raw_handler = handler_instance.get();
  raw_handler->set_disconnect_handler(base::BindOnce(
      [](AimEligibilityExtensionBridge* bridge,
         AimEligibilityPageHandler* handler_to_erase) {
        std::erase_if(bridge->page_handlers_,
                      [handler_to_erase](
                          const std::unique_ptr<AimEligibilityPageHandler>& p) {
                        return p.get() == handler_to_erase;
                      });
      },
      base::Unretained(this), raw_handler));
  page_handlers_.push_back(std::move(handler_instance));
}

}  // namespace extensions
