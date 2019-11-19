// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IN_PRODUCT_HELP_GLOBAL_MEDIA_CONTROLS_IN_PRODUCT_HELP_FACTORY_H_
#define CHROME_BROWSER_UI_IN_PRODUCT_HELP_GLOBAL_MEDIA_CONTROLS_IN_PRODUCT_HELP_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class GlobalMediaControlsInProductHelp;

class GlobalMediaControlsInProductHelpFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static GlobalMediaControlsInProductHelpFactory* GetInstance();

  static GlobalMediaControlsInProductHelp* GetForProfile(Profile* profile);

 private:
  GlobalMediaControlsInProductHelpFactory();
  ~GlobalMediaControlsInProductHelpFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  friend struct base::DefaultSingletonTraits<
      GlobalMediaControlsInProductHelpFactory>;

  DISALLOW_COPY_AND_ASSIGN(GlobalMediaControlsInProductHelpFactory);
};

#endif  // CHROME_BROWSER_UI_IN_PRODUCT_HELP_GLOBAL_MEDIA_CONTROLS_IN_PRODUCT_HELP_FACTORY_H_
