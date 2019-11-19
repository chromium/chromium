// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_data_service_factory.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/sql_init_error_message_ids.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Converts a WebDataServiceWrapper::ErrorType to ProfileErrorType.
ProfileErrorType ProfileErrorFromWebDataServiceWrapperError(
    WebDataServiceWrapper::ErrorType error_type) {
  switch (error_type) {
    case WebDataServiceWrapper::ERROR_LOADING_AUTOFILL:
      return ProfileErrorType::DB_AUTOFILL_WEB_DATA;

    case WebDataServiceWrapper::ERROR_LOADING_ACCOUNT_AUTOFILL:
      return ProfileErrorType::DB_ACCOUNT_AUTOFILL_WEB_DATA;

    case WebDataServiceWrapper::ERROR_LOADING_KEYWORD:
      return ProfileErrorType::DB_KEYWORD_WEB_DATA;

    case WebDataServiceWrapper::ERROR_LOADING_TOKEN:
      return ProfileErrorType::DB_TOKEN_WEB_DATA;

    case WebDataServiceWrapper::ERROR_LOADING_PASSWORD:
      return ProfileErrorType::DB_WEB_DATA;

    case WebDataServiceWrapper::ERROR_LOADING_PAYMENT_MANIFEST:
      return ProfileErrorType::DB_PAYMENT_MANIFEST_WEB_DATA;

    default:
      NOTREACHED() << "Unknown WebDataServiceWrapper::ErrorType: "
                   << error_type;
      return ProfileErrorType::DB_WEB_DATA;
  }
}

// Callback to show error dialog on profile load error.
void ProfileErrorCallback(WebDataServiceWrapper::ErrorType error_type,
                          sql::InitStatus status,
                          const std::string& diagnostics) {
  ShowProfileErrorDialog(ProfileErrorFromWebDataServiceWrapperError(error_type),
                         SqlInitStatusToMessageId(status), diagnostics);
}

}  // namespace

WebDataServiceFactory::WebDataServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WebDataService",
          BrowserContextDependencyManager::GetInstance()) {
  // WebDataServiceFactory has no dependecies.
}

WebDataServiceFactory::~WebDataServiceFactory() {}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // the *WebDataService::FromBrowserContext() functions (see above).
  DCHECK(access_type != ServiceAccessType::IMPLICIT_ACCESS ||
         !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfileIfExists(
    Profile* profile,
    ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // the *WebDataService::FromBrowserContext() functions (see above).
  DCHECK(access_type != ServiceAccessType::IMPLICIT_ACCESS ||
         !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceFactory::GetAutofillWebDataForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper ? wrapper->GetProfileAutofillWebData()
                 : scoped_refptr<autofill::AutofillWebDataService>(nullptr);
}

// static
scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceFactory::GetAutofillWebDataForAccount(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper ? wrapper->GetAccountAutofillWebData()
                 : scoped_refptr<autofill::AutofillWebDataService>(nullptr);
}

// static
scoped_refptr<KeywordWebDataService>
WebDataServiceFactory::GetKeywordWebDataForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper ? wrapper->GetKeywordWebData()
                 : scoped_refptr<KeywordWebDataService>(nullptr);
}

// static
scoped_refptr<TokenWebData> WebDataServiceFactory::GetTokenWebDataForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper ? wrapper->GetTokenWebData()
                 : scoped_refptr<TokenWebData>(nullptr);
}

// static
scoped_refptr<payments::PaymentManifestWebDataService>
WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper
             ? wrapper->GetPaymentManifestWebData()
             : scoped_refptr<payments::PaymentManifestWebDataService>(nullptr);
}

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  return base::Singleton<WebDataServiceFactory>::get();
}

content::BrowserContext* WebDataServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* WebDataServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  const base::FilePath& profile_path = context->GetPath();
  return new WebDataServiceWrapper(
      profile_path, g_browser_process->GetApplicationLocale(),
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}),
      base::BindRepeating(&ProfileErrorCallback));
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
