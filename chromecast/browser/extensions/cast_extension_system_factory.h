// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_SYSTEM_FACTORY_H_
#define CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_SYSTEM_FACTORY_H_

#include "base/memory/singleton.h"
#include "extensions/browser/extension_system_provider.h"

namespace extensions {

// A factory that provides CastExtensionSystem for cast_shell.
class CastExtensionSystemFactory : public ExtensionSystemProvider {
 public:
  // ExtensionSystemProvider implementation:
  ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) override;

  static CastExtensionSystemFactory* GetInstance();

  CastExtensionSystemFactory(const CastExtensionSystemFactory&) = delete;
  CastExtensionSystemFactory& operator=(const CastExtensionSystemFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<CastExtensionSystemFactory>;

  CastExtensionSystemFactory();
  ~CastExtensionSystemFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_SYSTEM_FACTORY_H_
