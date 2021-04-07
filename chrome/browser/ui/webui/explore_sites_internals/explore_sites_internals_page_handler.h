// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXPLORE_SITES_INTERNALS_EXPLORE_SITES_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXPLORE_SITES_INTERNALS_EXPLORE_SITES_INTERNALS_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace explore_sites {

// Concrete implementation of explore_sites_internals::mojom::PageHandler.
class ExploreSitesInternalsPageHandler
    : public explore_sites_internals::mojom::PageHandler {
 public:
  ExploreSitesInternalsPageHandler(
      mojo::PendingReceiver<explore_sites_internals::mojom::PageHandler>
          receiver,
      ExploreSitesService* explore_sites_service,
      Profile* profile);
  ~ExploreSitesInternalsPageHandler() override;

 private:
  // explore_sites_internals::mojom::ExploreSitesInternalsPageHandler
  void GetProperties(GetPropertiesCallback) override;
  void ClearCachedExploreSitesCatalog(
      ClearCachedExploreSitesCatalogCallback) override;
  void OverrideCountryCode(const std::string& country_code,
                           OverrideCountryCodeCallback) override;
  void ForceNetworkRequest(ForceNetworkRequestCallback) override;

  mojo::Receiver<explore_sites_internals::mojom::PageHandler> receiver_;
  ExploreSitesService* explore_sites_service_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesInternalsPageHandler);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_UI_WEBUI_EXPLORE_SITES_INTERNALS_EXPLORE_SITES_INTERNALS_PAGE_HANDLER_H_
