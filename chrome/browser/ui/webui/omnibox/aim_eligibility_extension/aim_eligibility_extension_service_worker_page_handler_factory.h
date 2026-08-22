// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_SERVICE_WORKER_PAGE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_SERVICE_WORKER_PAGE_HANDLER_FACTORY_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

class AimEligibilityPageHandler;
class Profile;

// Handles Mojo connection requests from AIM Eligibility extension service
// workers. This factory is bound to the profile lifecycle (via
// `AimEligibilityExtensionBridge`), so its lifetime and owned page handlers are
// automatically scoped to the Profile lifecycle as well as pipe disconnection.
class AimEligibilityExtensionServiceWorkerPageHandlerFactory
    : public aim_eligibility::mojom::PageHandlerFactory {
 public:
  explicit AimEligibilityExtensionServiceWorkerPageHandlerFactory(
      Profile& profile);
  ~AimEligibilityExtensionServiceWorkerPageHandlerFactory() override;

  void Bind(mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
                receiver);

  // Clears receivers and active page handlers. Called during profile shutdown.
  void Clear();

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
  absl::flat_hash_set<std::unique_ptr<AimEligibilityPageHandler>>
      page_handlers_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_SERVICE_WORKER_PAGE_HANDLER_FACTORY_H_
