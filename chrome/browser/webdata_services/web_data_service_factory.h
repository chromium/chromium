// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_SERVICES_WEB_DATA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_WEBDATA_SERVICES_WEB_DATA_SERVICE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class KeywordWebDataService;
class Profile;
class TokenWebData;
class WebDataServiceWrapper;

namespace autofill {
class AutofillWebDataService;
}

namespace plus_addresses {
class PlusAddressWebDataService;
}

// Singleton that owns all WebDataServiceWrappers and associates them with
// Profiles.
class WebDataServiceFactory
    : public webdata_services::WebDataServiceWrapperFactory {
 public:
  // Returns the WebDataServiceWrapper associated with the |profile|.
  static WebDataServiceWrapper* GetForProfile(Profile* profile,
                                              ServiceAccessType access_type);

  static WebDataServiceWrapper* GetForProfileIfExists(
      Profile* profile,
      ServiceAccessType access_type);

  WebDataServiceFactory(const WebDataServiceFactory&) = delete;
  WebDataServiceFactory& operator=(const WebDataServiceFactory&) = delete;

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

  // Returns the PlusAddressWebDataService associated with the `profile`.
  static scoped_refptr<plus_addresses::PlusAddressWebDataService>
  GetPlusAddressWebDataForProfile(Profile* profile,
                                  ServiceAccessType access_type);

  // Returns the TokenWebData associated with the |profile|.
  static scoped_refptr<TokenWebData> GetTokenWebDataForProfile(
      Profile* profile,
      ServiceAccessType access_type);

  static WebDataServiceFactory* GetInstance();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend base::NoDestructor<WebDataServiceFactory>;

  WebDataServiceFactory();
  ~WebDataServiceFactory() override;

  // |BrowserContextKeyedServiceFactory| methods:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_WEBDATA_SERVICES_WEB_DATA_SERVICE_FACTORY_H_
