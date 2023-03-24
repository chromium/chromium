// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Browser;
class CompanionSidePanelUntrustedUI;

namespace companion {
class CompanionUrlBuilder;
class PromoHandler;

class CompanionPageHandler : public side_panel::mojom::CompanionPageHandler,
                             public content::WebContentsObserver {
 public:
  explicit CompanionPageHandler(
      mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
      mojo::PendingRemote<side_panel::mojom::CompanionPage> page,
      Browser* browser,
      CompanionSidePanelUntrustedUI* search_companion_ui);
  CompanionPageHandler(const CompanionPageHandler&) = delete;
  CompanionPageHandler& operator=(const CompanionPageHandler&) = delete;
  ~CompanionPageHandler() override;

  // side_panel::mojom::CompanionPageHandler:
  void ShowUI() override;
  void OnPromoAction(side_panel::mojom::PromoType promo_type,
                     side_panel::mojom::PromoAction promo_action) override;
  void OnRegionSearchClicked() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  // Notifies the companion page of the visible URL when the active tab has
  // changed or when the primary page has changed on the active tab.
  void NotifyURLChanged();

  mojo::Receiver<side_panel::mojom::CompanionPageHandler> receiver_;
  mojo::Remote<side_panel::mojom::CompanionPage> page_;
  raw_ptr<CompanionSidePanelUntrustedUI> companion_untrusted_ui_ = nullptr;
  std::unique_ptr<CompanionUrlBuilder> url_builder_;
  std::unique_ptr<PromoHandler> promo_handler_;
};
}  // namespace companion

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_PAGE_HANDLER_H_
