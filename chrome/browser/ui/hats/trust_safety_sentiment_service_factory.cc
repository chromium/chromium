// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"

#include <iterator>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/language_tag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

TrustSafetySentimentServiceFactory::TrustSafetySentimentServiceFactory()
    : ProfileKeyedServiceFactory(
          "TrustSafetySentimentService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HatsServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

TrustSafetySentimentServiceFactory*
TrustSafetySentimentServiceFactory::GetInstance() {
  static base::NoDestructor<TrustSafetySentimentServiceFactory> instance;
  return instance.get();
}

TrustSafetySentimentService* TrustSafetySentimentServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<TrustSafetySentimentService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

void TrustSafetySentimentServiceFactory::ShutDownForTesting(
    content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

std::unique_ptr<KeyedService>
TrustSafetySentimentServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TrustSafetySentimentSurvey is conducted only for Windows, MacOS and Linux
  // currently.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS))

  // TrustSafetySentimentSurvey is conducted only in English currently.
  static constexpr auto kSurveyLocales =
      base::MakeFixedFlatSet<base::i18n::LanguageTag>(
          {base::i18n::GetKnownLanguageTag("en"),
           base::i18n::GetKnownLanguageTag("en-AU"),
           base::i18n::GetKnownLanguageTag("en-CA"),
           base::i18n::GetKnownLanguageTag("en-GB"),
           base::i18n::GetKnownLanguageTag("en-US")});
  if (!kSurveyLocales.contains(g_browser_process->GetFeatures()
                                   ->application_locale_storage()
                                   ->GetTag())) {
    return nullptr;
  }

  // TrustSafetySentimentSurvey is meant for users that are in a regular (not
  // incognito) mode.
  if (context->IsOffTheRecord() ||
      (!base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurvey) &&
       !base::FeatureList::IsEnabled(
           features::kTrustSafetySentimentSurveyV2))) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);

  // If there is no HaTS service, or the HaTS service reports the user is not
  // eligible to be surveyed by HaTS, do not create the service. This state is
  // unlikely to change over the life of the profile (e.g. before closing
  // Chrome) and simply not creating the service avoids unnecessary work
  // tracking user interactions.
  auto* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  if (!hats_service ||
      !hats_service->CanShowAnySurvey(/*user_prompted=*/false)) {
    return nullptr;
  }

  return std::make_unique<TrustSafetySentimentService>(profile);
#else
  return nullptr;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS))
}
