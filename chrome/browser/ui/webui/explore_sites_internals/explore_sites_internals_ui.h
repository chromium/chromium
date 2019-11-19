// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXPLORE_SITES_INTERNALS_EXPLORE_SITES_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_EXPLORE_SITES_INTERNALS_EXPLORE_SITES_INTERNALS_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals.mojom.h"
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
  ~ExploreSitesInternalsUI() override;

 private:
  void BindExploreSitesInternalsPageHandler(
      mojo::PendingReceiver<explore_sites_internals::mojom::PageHandler>
          receiver);

  std::unique_ptr<ExploreSitesInternalsPageHandler> page_handler_;
  ExploreSitesService* explore_sites_service_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesInternalsUI);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_UI_WEBUI_EXPLORE_SITES_INTERNALS_EXPLORE_SITES_INTERNALS_UI_H_
