// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "content/public/browser/browser_context.h"

namespace autofill {

// static
AutofillClientProviderFactory* AutofillClientProviderFactory::GetInstance() {
  static base::NoDestructor<AutofillClientProviderFactory> instance;
  return instance.get();
}

// static
AutofillClientProvider& AutofillClientProviderFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile) << "Autofill requires a valid profile.";
  auto* provider =
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true);
  CHECK(provider) << "Autofill is not available for the given profile.";
  return *static_cast<AutofillClientProvider*>(provider);
}

AutofillClientProviderFactory::AutofillClientProviderFactory()
    : ProfileKeyedServiceFactory(
          "AutofillClientProvider",
          // TODO: crbug.com/326231439 - Other/no provider for OTR profiles?
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

AutofillClientProviderFactory::~AutofillClientProviderFactory() = default;

std::unique_ptr<KeyedService>
AutofillClientProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AutofillClientProvider>(
      Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace autofill
