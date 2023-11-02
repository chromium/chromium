// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXPLORE_SITES_INTERNALS_EXPLORE_SITES_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_EXPLORE_SITES_INTERNALS_EXPLORE_SITES_INTERNALS_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals.mojom-forward.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals_page_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace explore_sites {

// UI controller for chrome://explore-sites-internals, hooks up a concrete
// implementation of explore_sites_internals::mojom::PageHandler to requests for
// that page handler that will come from the frontend.
class ExploreSitesInternalsUI : public ui::MojoWebUIController {
 public:
  explicit ExploreSitesInternalsUI(content::WebUI* web_ui);

  ExploreSitesInternalsUI(const ExploreSitesInternalsUI&) = delete;
  ExploreSitesInternalsUI& operator=(const ExploreSitesInternalsUI&) = delete;

  ~ExploreSitesInternalsUI() override;

  // Instantiates the implementor of the mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<explore_sites_internals::mojom::PageHandler>
          receiver);

 private:
  std::unique_ptr<ExploreSitesInternalsPageHandler> page_handler_;
  raw_ptr<ExploreSitesService> explore_sites_service_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_UI_WEBUI_EXPLORE_SITES_INTERNALS_EXPLORE_SITES_INTERNALS_UI_H_
