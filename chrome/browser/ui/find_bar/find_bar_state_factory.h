// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_FACTORY_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FindBarState;

class FindBarStateFactory : public ProfileKeyedServiceFactory {
 public:
  FindBarStateFactory(const FindBarStateFactory&) = delete;
  FindBarStateFactory& operator=(const FindBarStateFactory&) = delete;

  static FindBarState* GetForBrowserContext(content::BrowserContext* context);

  static FindBarStateFactory* GetInstance();

 private:
  friend base::NoDestructor<FindBarStateFactory>;

  FindBarStateFactory();
  ~FindBarStateFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_FACTORY_H_
