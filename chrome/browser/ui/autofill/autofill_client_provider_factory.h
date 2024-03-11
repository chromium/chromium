// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CLIENT_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CLIENT_PROVIDER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace autofill {
class AutofillClientProvider;

// A factory for creating one `AutofillClientProvider` per browser context. It's
// lazily created on first use. It provides a provider for off-the-record tabs.
class AutofillClientProviderFactory : public ProfileKeyedServiceFactory {
 public:
  static AutofillClientProviderFactory* GetInstance();
  static AutofillClientProvider& GetForProfile(Profile* profile);

  AutofillClientProviderFactory(const AutofillClientProviderFactory&) = delete;
  AutofillClientProviderFactory& operator=(
      const AutofillClientProviderFactory&) = delete;

 private:
  friend base::NoDestructor<AutofillClientProviderFactory>;

  AutofillClientProviderFactory();
  ~AutofillClientProviderFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CLIENT_PROVIDER_FACTORY_H_
