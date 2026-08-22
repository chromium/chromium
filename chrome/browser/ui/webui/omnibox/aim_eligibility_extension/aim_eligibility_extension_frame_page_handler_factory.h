// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_FRAME_PAGE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_FRAME_PAGE_HANDLER_FACTORY_H_

#include <memory>

#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class AimEligibilityPageHandler;

namespace content {
class RenderFrameHost;
}  // namespace content

// Handles Mojo connection requests from AIM Eligibility extension frames.
// This factory is bound to the document lifecycle (via
// `content::DocumentUserData`), so its lifetime and owned page handlers are
// automatically scoped to the `RenderFrameHost`.
class AimEligibilityExtensionFramePageHandlerFactory
    : public content::DocumentUserData<
          AimEligibilityExtensionFramePageHandlerFactory>,
      public aim_eligibility::mojom::PageHandlerFactory {
 public:
  ~AimEligibilityExtensionFramePageHandlerFactory() override;

  void Bind(mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
                receiver);

  // aim_eligibility::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<aim_eligibility::mojom::Page> page,
      mojo::PendingReceiver<aim_eligibility::mojom::PageHandler> handler)
      override;

  AimEligibilityPageHandler* page_handler_for_testing() const {
    return page_handler_.get();
  }

 private:
  explicit AimEligibilityExtensionFramePageHandlerFactory(
      content::RenderFrameHost* rfh);

  friend class content::DocumentUserData<
      AimEligibilityExtensionFramePageHandlerFactory>;
  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::Receiver<aim_eligibility::mojom::PageHandlerFactory> receiver_{this};
  std::unique_ptr<AimEligibilityPageHandler> page_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_FRAME_PAGE_HANDLER_FACTORY_H_
