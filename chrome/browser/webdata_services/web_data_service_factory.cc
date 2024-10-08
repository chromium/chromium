// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata_services/web_data_service_factory.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/sql_init_error_message_ids.h"
#include "chrome/browser/ui/profiles/profile_error_dialog.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

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

    case WebDataServiceWrapper::ERROR_LOADING_PLUS_ADDRESS:
      return ProfileErrorType::DB_WEB_DATA;

    default:
      NOTREACHED_IN_MIGRATION()
          << "Unknown WebDataServiceWrapper::ErrorType: " << error_type;
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

// Predicate that determines whether autofill should use an in-memory database
// for account data (i.e. data corresponding to signed-in-non-syncing users) as
// opposed to on-disk storage.
//
// The desired product behavior in the strictest form would lead to returning
// different values during the lifetime of a profile, if the user's sign-in
// state changes. However, it is possible to make a good decision during profile
// startup, based on IdentityManager's state machine and its representation in
// prefs.
bool ShouldUseInMemoryAutofillAccountDatabase(PrefService* pref_service) {
#if BUILDFLAG(IS_ANDROID)
  // On Android (and iOS), the account storage is persisted on disk.
  return false;
#else   // BUILDFLAG(IS_ANDROID)
  // Historically, and before the flag rollout represented by the predicate
  // below, desktop platforms have used an in-memory database for autofill
  // account data.
  if (!switches::IsImprovedSigninUIOnDesktopEnabled()) {
    return true;
  }
  // The interpretation of the pref mimics what PrimaryAccountManager's
  // constructor does.
  const bool is_signed_in =
      !pref_service->GetString(::prefs::kGoogleServicesAccountId).empty();
  // If the user is signed out during profile startup, as per switch above
  // being enabled, any new sign-ins will involve an explicit sign-in (i.e.
  // interaction with native UI). In this case, on-disk storage is appropriate.
  if (!is_signed_in) {
    return false;
  }
  // It is possible that the user already is in an explicit sign-in state. In
  // this case, on-disk storage is appropriate, as any additional future
  // sign-ins (if the user first signs out) are guaranteed to be explicit
  // sign-ins too.
  if (pref_service->GetBoolean(::prefs::kExplicitBrowserSignin)) {
    return false;
  }
  // The interpretation of the pref mimics what PrimaryAccountManager's
  // constructor does.
  const bool is_consented_to_sync =
      pref_service->GetBoolean(::prefs::kGoogleServicesConsentedToSync);
  // If Sync (the feature) is on, the account storage isn't currently used. This
  // is because the only way to activate the account storate requires signing
  // out first, which means the predicate can return false as per earlier
  // rationale. With one exception: managed profiles may turn sync off without
  // signing out. Either way, having turned sync on implies the user interacted
  // explicitly with a sync UI, so in this particular context it is no different
  // from explicit sign-in, and on-disk storage is appropriate.
  if (is_consented_to_sync) {
    return false;
  }
  // The remaining case implies a legacy signed-in-non-syncing state with
  // implicit sign-in, which means the user signed in before the latest feature
  // flags rolled out. This is the only case where in-memory storage should be
  // used.
  //
  // Note that, during the lifetime of the browser/profile, it is still possible
  // that the users signs out and signs back in, where the latter is guaranteed
  // to be an explicit sign-in. In this case, it would be theoretically better
  // to immediately switch to on-disk storage, but this isn't possible once a
  // profile is initialized (as this predicate only gets evaluated once).
  // Conveniently, it is also harmless to use the in-memory storage until the
  // next browser restart, given that this is a one-off transition (upon restart
  // the code would run into one of the cases listed earlier that return false).
  return true;
#endif  // BUILDFLAG(IS_ANDROID)
}

std::unique_ptr<KeyedService> BuildWebDataService(
    content::BrowserContext* context) {
  const base::FilePath& profile_path = context->GetPath();
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<WebDataServiceWrapper>(
      profile_path, g_browser_process->GetApplicationLocale(),
      content::GetUIThreadTaskRunner({}),
      base::BindRepeating(&ProfileErrorCallback),
      g_browser_process->os_crypt_async(),
      ShouldUseInMemoryAutofillAccountDatabase(profile->GetPrefs()));
}

}  // namespace

WebDataServiceFactory::WebDataServiceFactory() = default;

WebDataServiceFactory::~WebDataServiceFactory() = default;

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  return GetForBrowserContext(profile, access_type);
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfileIfExists(
    Profile* profile,
    ServiceAccessType access_type) {
  return GetForBrowserContextIfExists(profile, access_type);
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
scoped_refptr<plus_addresses::PlusAddressWebDataService>
WebDataServiceFactory::GetPlusAddressWebDataForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper ? wrapper->GetPlusAddressWebData()
                 : scoped_refptr<plus_addresses::PlusAddressWebDataService>(
                       nullptr);
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
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  static base::NoDestructor<WebDataServiceFactory> instance;
  return instance.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
WebDataServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildWebDataService);
}

content::BrowserContext* WebDataServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

std::unique_ptr<KeyedService>
WebDataServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildWebDataService(context);
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
