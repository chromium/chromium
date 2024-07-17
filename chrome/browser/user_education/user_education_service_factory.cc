// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_service_factory.h"

#include <memory>
#include <optional>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_education/common/feature_promo_idle_observer.h"
#include "components/user_education/common/feature_promo_idle_policy.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

namespace {

// Idle observer that doesn't do anything.
class StubIdleObserver : public user_education::FeaturePromoIdleObserver {
 public:
  StubIdleObserver() = default;
  ~StubIdleObserver() override = default;

  // FeaturePromoIdleObserver:
  std::optional<base::Time> MaybeGetNewLastActiveTime() const override {
    return std::nullopt;
  }
};

}  // namespace

// These are found in chrome/browser/ui/user_education, so extern the factory
// methods to create the necessary objects.
extern std::unique_ptr<user_education::FeaturePromoIdleObserver>
CreatePollingIdleObserver();
extern std::unique_ptr<RecentSessionObserver> CreateRecentSessionObserver(
    Profile& profile);

UserEducationServiceFactory* UserEducationServiceFactory::GetInstance() {
  static base::NoDestructor<UserEducationServiceFactory> instance;
  return instance.get();
}

// static
UserEducationService* UserEducationServiceFactory::GetForBrowserContext(
    content::BrowserContext* profile) {
  return static_cast<UserEducationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UserEducationServiceFactory::UserEducationServiceFactory()
    : ProfileKeyedServiceFactory(
          "UserEducationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // The service is needed by the System Profile OTR (that manages
              // the Profile Picker) to control the IPHs displayed in the
              // Profile Picker.
              .WithSystem(ProfileSelection::kOffTheRecordOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

UserEducationServiceFactory::~UserEducationServiceFactory() = default;

// static
std::unique_ptr<UserEducationService>
UserEducationServiceFactory::BuildServiceInstanceForBrowserContextImpl(
    content::BrowserContext* context,
    bool disable_idle_polling) {
  Profile* const profile = Profile::FromBrowserContext(context);

  // Create the user education service.
  auto result = std::make_unique<UserEducationService>(
      std::make_unique<BrowserFeaturePromoStorageService>(profile),
      ProfileAllowsUserEducation(profile));

  // Set up the session manager.
  result->feature_promo_session_manager().Init(
      &result->feature_promo_storage_service(),
      disable_idle_polling ? std::make_unique<StubIdleObserver>()
                           : CreatePollingIdleObserver(),
      std::make_unique<user_education::FeaturePromoIdlePolicy>());

  // Possibly install a session observer. This isn't public, since it's
  // self-contained and mostly for tracking state.
  if (result->recent_session_tracker()) {
    result->recent_session_observer_ = CreateRecentSessionObserver(*profile);
    result->recent_session_observer_->Init(*result->recent_session_tracker());
  }

  return result;
}

// static
bool UserEducationServiceFactory::ProfileAllowsUserEducation(Profile* profile) {
#if BUILDFLAG(CHROME_FOR_TESTING)
  // IPH is always disabled in Chrome for Testing.
  return false;
#else
  // In order to do user education, the browser must have a UI and not be an
  // "off-the-record" or in a demo or guest mode.
  if (profile->IsIncognitoProfile() || profile->IsGuestSession() ||
      profiles::IsDemoSession() || profiles::IsChromeAppKioskSession()) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::IsManagedGuestSession()) {
    return false;
  }
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (profiles::IsWebKioskSession()) {
    return false;
  }
#endif
  if (headless::IsHeadlessMode()) {
    return false;
  }
  return true;
#endif  // BUILDFLAG(CHROME_FOR_TESTING)
}

std::unique_ptr<KeyedService>
UserEducationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildServiceInstanceForBrowserContextImpl(context,
                                                   disable_idle_polling_);
}
