// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_FACTORY_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_FACTORY_H_

#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class FindBarState;

class FindBarStateFactory : public BrowserContextKeyedServiceFactory {
 public:
  static FindBarState* GetForBrowserContext(content::BrowserContext* context);

  static FindBarStateFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<FindBarStateFactory>;

  FindBarStateFactory();
  ~FindBarStateFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(FindBarStateFactory);
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_FACTORY_H_
