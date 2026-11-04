// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class OmniboxPopupWebUIPool;
class Profile;

// Creates the `OmniboxPopupWebUIPool` of a profile. Incognito and guest
// profiles get their own pool, so popups never move between profiles.
class OmniboxPopupWebUIPoolFactory : public ProfileKeyedServiceFactory {
 public:
  static OmniboxPopupWebUIPoolFactory* GetInstance();
  static OmniboxPopupWebUIPool* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<OmniboxPopupWebUIPoolFactory>;

  OmniboxPopupWebUIPoolFactory();
  ~OmniboxPopupWebUIPoolFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_FACTORY_H_
