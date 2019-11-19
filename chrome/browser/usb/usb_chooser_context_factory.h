// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class UsbChooserContext;
class Profile;

class UsbChooserContextFactory : public BrowserContextKeyedServiceFactory {
 public:
  static UsbChooserContext* GetForProfile(Profile* profile);
  static UsbChooserContextFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<UsbChooserContextFactory>;

  UsbChooserContextFactory();
  ~UsbChooserContextFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(UsbChooserContextFactory);
};

#endif  // CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_FACTORY_H_
