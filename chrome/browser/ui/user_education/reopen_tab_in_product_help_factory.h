// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_REOPEN_TAB_IN_PRODUCT_HELP_FACTORY_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_REOPEN_TAB_IN_PRODUCT_HELP_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class ReopenTabInProductHelp;

class ReopenTabInProductHelpFactory : public ProfileKeyedServiceFactory {
 public:
  ReopenTabInProductHelpFactory(const ReopenTabInProductHelpFactory&) = delete;
  ReopenTabInProductHelpFactory& operator=(
      const ReopenTabInProductHelpFactory&) = delete;

  static ReopenTabInProductHelpFactory* GetInstance();

  static ReopenTabInProductHelp* GetForProfile(Profile* profile);

 private:
  ReopenTabInProductHelpFactory();
  ~ReopenTabInProductHelpFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  friend struct base::DefaultSingletonTraits<ReopenTabInProductHelpFactory>;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_REOPEN_TAB_IN_PRODUCT_HELP_FACTORY_H_
