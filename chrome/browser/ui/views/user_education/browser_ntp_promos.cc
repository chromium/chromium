// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_ntp_promos.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/browser/user_education/ntp_promo_identifiers.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_metadata.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension_urls.h"
#include "ui/actions/actions.h"

using user_education::NtpPromoContent;
using user_education::NtpPromoSpecification;

namespace {

using ContextPtr = const user_education::UserEducationContextPtr&;

Profile* GetProfile(ContextPtr context) {
  return context->AsA<BrowserUserEducationContext>()
      ->GetBrowserView()
      .GetProfile();
}

NtpPromoSpecification::Eligibility CheckSignInPromoEligibility(
    ContextPtr context) {
  // TODO(webium): add user education context for WebUI browser.
  if (!context) {
    return NtpPromoSpecification::Eligibility::kIneligible;
  }

  auto* profile = GetProfile(context);
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    return NtpPromoSpecification::Eligibility::kIneligible;
  }

  const auto signed_in_state = signin_util::GetSignedInState(
      IdentityManagerFactory::GetForProfile(profile));
  switch (signed_in_state) {
    case signin_util::SignedInState::kSignedOut:
      // User is fully signed out.
      return NtpPromoSpecification::Eligibility::kEligible;
    case signin_util::SignedInState::kWebOnlySignedIn:
      // When signed in on the web, one-click sign in options exist elsewhere
      // in Chrome. This promo currently only offers the full-sign-in flow, so
      // don't show it to users already signed in on the Web.
      return NtpPromoSpecification::Eligibility::kIneligible;
    case signin_util::SignedInState::kSignedIn:
    case signin_util::SignedInState::kSyncing:
    case signin_util::SignedInState::kSignInPending:
    case signin_util::SignedInState::kSyncPaused:
      // All other cases are considered completed.
      return NtpPromoSpecification::Eligibility::kCompleted;
  }
}

void SignInPromoShown() {
  signin_metrics::LogSignInOffered(
      signin_metrics::AccessPoint::kNtpFeaturePromo,
      signin_metrics::PromoAction::
          PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
}

void InvokeSignInPromo(ContextPtr context) {
  // Note that this invokes a "from scratch" sign-in flow, even if the user is
  // already signed in on the Web. Later, we can evolve this if desired to
  // offer an alternate one-click sign-in flow for those other users.
  signin_ui_util::ShowSigninPromptFromPromo(
      GetProfile(context), signin_metrics::AccessPoint::kNtpFeaturePromo);
}

NtpPromoSpecification::Eligibility CheckExtensionsPromoEligibility(
    ContextPtr context) {
  // TODO(webium): add user education context for WebUI browser.
  if (!context) {
    return NtpPromoSpecification::Eligibility::kIneligible;
  }
  return extensions::util::AnyCurrentlyInstalledExtensionIsFromWebstore(
             GetProfile(context))
             ? NtpPromoSpecification::Eligibility::kCompleted
             : NtpPromoSpecification::Eligibility::kEligible;
}

void InvokeExtensionsPromo(ContextPtr context) {
  std::string_view utm_source;
  switch (user_education::features::GetNtpBrowserPromoType()) {
    case user_education::features::NtpBrowserPromoType::kSimple:
      if (user_education::features::GetNtpBrowserPromoIndividualPromoLimit() >
          1) {
        utm_source = extension_urls::kNtpPromo2pUtmSource;
      } else {
        utm_source = extension_urls::kNtpPromo1pUtmSource;
      }
      break;
    case user_education::features::NtpBrowserPromoType::kSetupList:
      utm_source = extension_urls::kNtpPromoSlUtmSource;
      break;
    default:
      NOTREACHED();
  }

  GURL url_with_utm = extension_urls::AppendUtmSource(
      extension_urls::GetWebstoreLaunchURL(), utm_source);

  // TODO(crbug.com/443062679): Use the BrowserWindowInterface version when
  // it becomes available.
  NavigateParams params(
      context->AsA<BrowserUserEducationContext>()->GetBrowserView().browser(),
      url_with_utm, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  Navigate(&params);
}

NtpPromoSpecification::Eligibility CheckCustomizationPromoEligibility(
    ContextPtr context) {
  // TODO(webium): add user education context for WebUI browser.
  if (!context) {
    return NtpPromoSpecification::Eligibility::kIneligible;
  }
  auto* profile = GetProfile(context);
  auto* background_service =
      NtpCustomBackgroundServiceFactory::GetForProfile(profile);
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
  if (!background_service || !theme_service) {
    return NtpPromoSpecification::Eligibility::kIneligible;
  }
  // Infer that if a custom background or theme is used, customization has
  // taken place.
  const bool customized =
      background_service->GetCustomBackground().has_value() ||
      (theme_service->GetThemeID() != ThemeHelper::kDefaultThemeID);
  return customized ? NtpPromoSpecification::Eligibility::kCompleted
                    : NtpPromoSpecification::Eligibility::kEligible;
}

void InvokeCustomizationPromo(ContextPtr context) {
  actions::ActionManager::Get()
      .FindAction(kActionSidePanelShowCustomizeChrome)
      ->InvokeAction(
          actions::ActionInvocationContext::Builder()
              .SetProperty(
                  kSidePanelOpenTriggerKey,
                  static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                      SidePanelOpenTrigger::kNewTabPageCustomizationPromo))
              .Build());
}

}  // namespace

void MaybeRegisterNtpPromos(user_education::NtpPromoRegistry& registry) {
  if (registry.AreAnyPromosRegistered()) {
    return;
  }

  // Register NTP Promos below.
  //
  // Absent MRU/LRU and explicit `show_after` parameters, promos will be shown
  // in the order they appear here, so pay careful attention to what order new
  // users should see promos in (especially as not all promos may be able to
  // display at once).
  //
  // NOTE: Changes to this file should be reviewed by both a User Education
  // owner (//components/user_education/OWNERS) and an NTP owner
  // (//components/search/OWNERS).

  registry.AddPromo(NtpPromoSpecification(
      kNtpSignInPromoId,
      NtpPromoContent("account_circle", IDS_NTP_SIGN_IN_PROMO_WITH_BOOKMARKS,
                      IDS_NTP_SIGN_IN_PROMO_WITH_BOOKMARKS),
      base::BindRepeating(&CheckSignInPromoEligibility),
      base::BindRepeating(&SignInPromoShown),
      base::BindRepeating(&InvokeSignInPromo),
      /*show_after=*/{},
      user_education::Metadata(
          141, "cjgrant@google.com",
          "Promotes sign-in capability on the New Tab Page")));

  registry.AddPromo(NtpPromoSpecification(
      kNtpCustomizationPromoId,
      NtpPromoContent("palette", IDS_NTP_CUSTOMIZATION_PROMO,
                      IDS_NTP_CUSTOMIZATION_PROMO),
      base::BindRepeating(&CheckCustomizationPromoEligibility),
      /*show_callback=*/base::DoNothing(),
      base::BindRepeating(&InvokeCustomizationPromo),
      /*show_after=*/{},
      user_education::Metadata(141, "cjgrant@google.com",
                               "Promotes customization of the New Tab Page")));

  registry.AddPromo(NtpPromoSpecification(
      kNtpExtensionsPromoId,
      NtpPromoContent("my_extensions", IDS_NTP_EXTENSIONS_PROMO,
                      IDS_NTP_EXTENSIONS_PROMO),
      base::BindRepeating(&CheckExtensionsPromoEligibility),
      /*show_callback=*/base::DoNothing(),
      base::BindRepeating(&InvokeExtensionsPromo),
      /*show_after=*/{},
      user_education::Metadata(
          141, "cjgrant@google.com",
          "Promotes Chrome extensions on the New Tab Page")));
}
