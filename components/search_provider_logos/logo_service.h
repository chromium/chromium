// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_SERVICE_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/search_provider_logos/logo_common.h"

namespace search_provider_logos {

class LogoObserver;

// Provides the logo for a profile's default search provider.
//
// Example usage:
//   LogoService* logo_service = LogoServiceFactory::GetForProfile(profile);
//   LogoCallbacks callbacks;
//   callbacks.on_cached_decoded_logo = base::BindOnce(ShowLogo);
//   callbacks.on_fresh_decoded_logo = base::BindOnce(FadeToLogo);
//   logo_service->GetLogo(callbacks);
//
class LogoService : public KeyedService {
 public:
  LogoService(const LogoService&) = delete;
  LogoService& operator=(const LogoService&) = delete;

  // Gets the logo for the default search provider and calls the provided
  // callbacks with the encoded and decoded logos. The service will:
  //
  // 1.  Load a cached logo, and call callbacks.on_cached_{en,de}coded_logo.
  // 2.  Fetch a fresh logo, and call callbacks.on_fresh_{en,de}coded_logo.
  //
  // At least one member of |callbacks| must be non-null. If |for_webui_ntp| is
  // true fetches a logo that is compatible with the WebUI NTP.
  virtual void GetLogo(LogoCallbacks callbacks, bool for_webui_ntp) = 0;

  // Gets the logo for the default search provider and notifies |observer|
  // 0-2 times with the results. The service will:
  //
  // 1.  Call observer->OnLogoAvailable() with |from_cache=true| when
  //     |on_cached_decoded_logo_available| would be called in the callback
  //     interface with type DETERMINED.
  // 2.  Call observer->OnLogoAvailable() with |from_cache=false| when
  //     |on_fresh_decoded_logo_available| would be called in the callback
  //     interface with type DETERMINED.
  // 3.  Call observer->OnObserverRemoved().
  virtual void GetLogo(LogoObserver* observer) = 0;

 protected:
  LogoService();
};

}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_SERVICE_H_
