// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BRIDGE_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BRIDGE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class Profile;
class AimEligibilityPageHandler;

namespace extensions {

// A KeyedService that hosts AimEligibilityPageHandler instances for component
// extensions via direct Mojo binding.
class AimEligibilityExtensionBridge
    : public KeyedService,
      public aim_eligibility::mojom::PageHandlerFactory {
 public:
  explicit AimEligibilityExtensionBridge(Profile* profile);

  AimEligibilityExtensionBridge(const AimEligibilityExtensionBridge&) = delete;
  AimEligibilityExtensionBridge& operator=(
      const AimEligibilityExtensionBridge&) = delete;

  ~AimEligibilityExtensionBridge() override;

  // KeyedService:
  void Shutdown() override;

  static AimEligibilityExtensionBridge* Get(Profile* profile);

  void BindFactoryReceiver(
      mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
          receiver);

  // aim_eligibility::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<aim_eligibility::mojom::Page> page,
      mojo::PendingReceiver<aim_eligibility::mojom::PageHandler> handler)
      override;

  size_t page_handlers_size_for_testing() const {
    return page_handlers_.size();
  }

 private:
  const raw_ref<Profile> profile_;
  mojo::ReceiverSet<aim_eligibility::mojom::PageHandlerFactory> receivers_;
  std::vector<std::unique_ptr<AimEligibilityPageHandler>> page_handlers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BRIDGE_H_
