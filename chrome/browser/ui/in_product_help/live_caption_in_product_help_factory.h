// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IN_PRODUCT_HELP_LIVE_CAPTION_IN_PRODUCT_HELP_FACTORY_H_
#define CHROME_BROWSER_UI_IN_PRODUCT_HELP_LIVE_CAPTION_IN_PRODUCT_HELP_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class LiveCaptionInProductHelp;
class Profile;

class LiveCaptionInProductHelpFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static LiveCaptionInProductHelp* GetForProfile(Profile* profile);

  static LiveCaptionInProductHelpFactory* GetInstance();

 private:
  friend base::NoDestructor<LiveCaptionInProductHelpFactory>;

  LiveCaptionInProductHelpFactory();
  ~LiveCaptionInProductHelpFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_UI_IN_PRODUCT_HELP_LIVE_CAPTION_IN_PRODUCT_HELP_FACTORY_H_
