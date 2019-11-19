// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_DATA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_WEB_DATA_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class KeywordWebDataService;
class Profile;
class TokenWebData;
class WebDataServiceWrapper;

namespace payments {
class PaymentManifestWebDataService;
}

namespace autofill {
class AutofillWebDataService;
}

// Singleton that owns all WebDataServiceWrappers and associates them with
// Profiles.
class WebDataServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the WebDataServiceWrapper associated with the |profile|.
  static WebDataServiceWrapper* GetForProfile(Profile* profile,
                                              ServiceAccessType access_type);

  static WebDataServiceWrapper* GetForProfileIfExists(
      Profile* profile,
      ServiceAccessType access_type);

  // Returns the AutofillWebDataService associated with the |profile|.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForProfile(Profile* profile, ServiceAccessType access_type);

  // Returns the account-scoped AutofillWebDataService associated with the
  // |profile|.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForAccount(Profile* profile, ServiceAccessType access_type);

  // Returns the KeywordWebDataService associated with the |profile|.
  static scoped_refptr<KeywordWebDataService> GetKeywordWebDataForProfile(
      Profile* profile,
      ServiceAccessType access_type);

  // Returns the TokenWebData associated with the |profile|.
  static scoped_refptr<TokenWebData> GetTokenWebDataForProfile(
      Profile* profile,
      ServiceAccessType access_type);

  static scoped_refptr<payments::PaymentManifestWebDataService>
  GetPaymentManifestWebDataForProfile(Profile* profile,
                                      ServiceAccessType access_type);

  static WebDataServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WebDataServiceFactory>;

  WebDataServiceFactory();
  ~WebDataServiceFactory() override;

  // |BrowserContextKeyedServiceFactory| methods:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceFactory);
};

#endif  // CHROME_BROWSER_WEB_DATA_SERVICE_FACTORY_H_
