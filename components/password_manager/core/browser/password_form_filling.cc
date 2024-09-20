// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_filling.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"

namespace password_manager {

namespace {

using affiliations::FacetURI;
using autofill::PasswordAndMetadata;
using autofill::PasswordFormFillData;
using url::Origin;
using Logger = autofill::SavePasswordProgressLogger;
using password_manager_util::GetMatchType;
using GetLoginMatchType = password_manager_util::GetLoginMatchType;

bool PreferredRealmIsFromAndroid(const PasswordFormFillData& fill_data) {
  return FacetURI::FromPotentiallyInvalidSpec(fill_data.preferred_login.realm)
      .IsValidAndroidFacetURI();
}

bool ContainsAndroidCredentials(const PasswordFormFillData& fill_data) {
  for (const auto& login : fill_data.additional_logins) {
    if (FacetURI::FromPotentiallyInvalidSpec(login.realm)
            .IsValidAndroidFacetURI()) {
      return true;
    }
  }

  return PreferredRealmIsFromAndroid(fill_data);
}

#if !BUILDFLAG(IS_IOS) && !defined(ANDROID)
bool IsFillOnAccountSelectFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kFillOnAccountSelect);
}
#endif

void Autofill(PasswordManagerClient* client,
              PasswordManagerDriver* driver,
              const PasswordForm& form_for_autofill,
              base::span<const PasswordForm> best_matches,
              base::span<const PasswordForm> federated_matches,
              std::optional<PasswordForm> preferred_match,
              bool wait_for_username,
              base::span<autofill::FieldRendererId> suggestion_banned_fields) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client->GetLogManager());
    logger->LogMessage(Logger::STRING_PASSWORDMANAGER_AUTOFILL);
  }

  PasswordFormFillData fill_data = CreatePasswordFormFillData(
      form_for_autofill, best_matches, std::move(preferred_match),
      client->GetLastCommittedOrigin(), wait_for_username,
      suggestion_banned_fields);
  if (logger) {
    logger->LogBoolean(Logger::STRING_WAIT_FOR_USERNAME, wait_for_username);
  }
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.FillSuggestionsIncludeAndroidAppCredentials",
      ContainsAndroidCredentials(fill_data));
  if (!wait_for_username) {
    metrics_util::LogFilledPasswordFromAndroidApp(
        PreferredRealmIsFromAndroid(fill_data));
  }
  driver->SetPasswordFillData(fill_data);

  // Matches can be empty when there are only WebAuthn credentials available.
  // In that case there will be no actual fill so the client doesn't need
  // to be notified.
  if (!best_matches.empty() || !federated_matches.empty()) {
    client->PasswordWasAutofilled(best_matches,
                                  Origin::Create(form_for_autofill.url),
                                  federated_matches, !wait_for_username);
  }
}

std::string GetPreferredRealm(const PasswordForm& form) {
  return form.app_display_name.empty() ? form.signon_realm
                                       : form.app_display_name;
}

bool IsSameOrigin(const Origin& frame_origin, const GURL& credential_url) {
  return frame_origin.IsSameOriginWith(Origin::Create(credential_url));
}

}  // namespace

LikelyFormFilling SendFillInformationToRenderer(
    PasswordManagerClient* client,
    PasswordManagerDriver* driver,
    const PasswordForm& observed_form,
    base::span<const PasswordForm> best_matches,
    base::span<const PasswordForm> federated_matches,
    const PasswordForm* preferred_match,
    PasswordFormMetricsRecorder* metrics_recorder,
    bool webauthn_suggestions_available,
    base::span<autofill::FieldRendererId> suggestion_banned_fields) {
  DCHECK(driver);
  DCHECK_EQ(PasswordForm::Scheme::kHtml, observed_form.scheme);

  if (autofill::IsShowAutofillSignaturesEnabled()) {
    driver->AnnotateFieldsWithParsingResult(
        {.username_renderer_id = observed_form.username_element_renderer_id,
         .password_renderer_id = observed_form.password_element_renderer_id,
         .new_password_renderer_id =
             observed_form.new_password_element_renderer_id,
         .confirm_password_renderer_id =
             observed_form.confirmation_password_element_renderer_id});
  }

  if (best_matches.empty() && !webauthn_suggestions_available) {
    bool should_show_popup_without_passwords =
        client->IsSavingAndFillingEnabled(observed_form.url) &&
        (client->GetPasswordFeatureManager()->ShouldShowAccountStorageOptIn() ||
         client->GetPasswordFeatureManager()->ShouldShowAccountStorageReSignin(
             client->GetLastCommittedURL()));

    driver->InformNoSavedCredentials(should_show_popup_without_passwords);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventNoCredential);
    return LikelyFormFilling::kNoFilling;
  }

  // The only case in which there is no preferred_match is if there are no
  // saved passwords but there are WebAuthn credentials that can be presented.
  DCHECK(preferred_match || webauthn_suggestions_available);

  // If the parser of the PasswordFormManager decides that there is no
  // current password field, no filling attempt will be made. In this case the
  // renderer won't treat this as the "first filling" and won't record metrics
  // accordingly. The browser should not do that either.
  const bool not_sign_in_form =
      !observed_form.HasPasswordElement() && !observed_form.IsSingleUsername();

  if (preferred_match && !not_sign_in_form) {
    metrics_recorder->RecordMatchedFormType(*preferred_match);
  }

// This metric will always record kReauthRequired on iOS and Android. So we can
// drop it there.
#if !BUILDFLAG(IS_IOS) && !defined(ANDROID)
  // Proceed to autofill.
  // Note that we provide the choices but don't actually prefill a value if:
  // (1) we are in Incognito mode, or
  // (2) if it matched using public suffix domain matching, or
  // (3) if is matched by the `AffiliationService`, or
  // (4) it would result in unexpected filling in a form with new password
  //     fields.
  using WaitForUsernameReason =
      PasswordFormMetricsRecorder::WaitForUsernameReason;
  WaitForUsernameReason wait_for_username_reason =
      WaitForUsernameReason::kDontWait;
  if (client->IsOffTheRecord()) {
    wait_for_username_reason = WaitForUsernameReason::kIncognitoMode;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  } else if (client->GetPasswordFeatureManager()
                 ->IsBiometricAuthenticationBeforeFillingEnabled()) {
    wait_for_username_reason = WaitForUsernameReason::kBiometricAuthentication;
#endif
  } else if (preferred_match &&
             GetMatchType(*preferred_match) == GetLoginMatchType::kAffiliated &&
             !affiliations::IsValidAndroidFacetURI(
                 preferred_match->signon_realm)) {
    wait_for_username_reason = WaitForUsernameReason::kAffiliatedWebsite;
  } else if (preferred_match &&
             GetMatchType(*preferred_match) == GetLoginMatchType::kPSL) {
    wait_for_username_reason = WaitForUsernameReason::kPublicSuffixMatch;
  } else if (preferred_match &&
             GetMatchType(*preferred_match) == GetLoginMatchType::kGrouped) {
    wait_for_username_reason = WaitForUsernameReason::kGroupedMatch;
  } else if (!IsSameOrigin(client->GetLastCommittedOrigin(),
                           GURL(observed_form.signon_realm))) {
    wait_for_username_reason = WaitForUsernameReason::kCrossOriginIframe;
  } else if (not_sign_in_form) {
    // If the parser did not find a current password element, don't fill.
    wait_for_username_reason = WaitForUsernameReason::kFormNotGoodForFilling;
  } else if (observed_form.HasUsernameElement() &&
             observed_form.HasNonEmptyPasswordValue() &&
             observed_form.server_side_classification_successful &&
             !observed_form.username_may_use_prefilled_placeholder) {
    // Password is already filled in and we don't think the username is a
    // placeholder, so don't overwrite.
    wait_for_username_reason = WaitForUsernameReason::kPasswordPrefilled;
  } else if (!client->IsCommittedMainFrameSecure()) {
    wait_for_username_reason = WaitForUsernameReason::kInsecureOrigin;
  } else if (IsFillOnAccountSelectFeatureEnabled()) {
    wait_for_username_reason = WaitForUsernameReason::kFoasFeature;
  } else if (observed_form.accepts_webauthn_credentials) {
    wait_for_username_reason =
        WaitForUsernameReason::kAcceptsWebAuthnCredentials;
  }

  // Record no "FirstWaitForUsernameReason" metrics for a form that is not meant
  // for filling. The renderer won't record a "FirstFillingResult" either.
  if (!not_sign_in_form) {
    metrics_recorder->RecordFirstWaitForUsernameReason(
        wait_for_username_reason);
  }

  bool wait_for_username =
      wait_for_username_reason != WaitForUsernameReason::kDontWait;
#else
  bool wait_for_username = true;
#endif  // !BUILDFLAG(IS_IOS) && !defined(ANDROID)

  if (wait_for_username) {
    metrics_recorder->SetManagerAction(
        PasswordFormMetricsRecorder::kManagerActionNone);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventBlockedOnInteraction);
  } else {
    metrics_recorder->SetManagerAction(
        PasswordFormMetricsRecorder::kManagerActionAutofilled);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventAutofilled);
    base::RecordAction(base::UserMetricsAction("PasswordManager_Autofilled"));
  }

  // Continue with autofilling any password forms as traditionally has been
  // done.
  Autofill(
      client, driver, observed_form, best_matches, federated_matches,
      preferred_match ? std::make_optional(*preferred_match) : std::nullopt,
      wait_for_username, suggestion_banned_fields);

  return wait_for_username ? LikelyFormFilling::kFillOnAccountSelect
                           : LikelyFormFilling::kFillOnPageLoad;
}

PasswordFormFillData CreatePasswordFormFillData(
    const PasswordForm& form_on_page,
    base::span<const PasswordForm> matches,
    std::optional<PasswordForm> preferred_match,
    const Origin& main_frame_origin,
    bool wait_for_username,
    base::span<autofill::FieldRendererId> suggestion_banned_fields) {
  PasswordFormFillData result;

  result.form_renderer_id = form_on_page.form_data.renderer_id();
  result.url = form_on_page.url;
  result.wait_for_username = wait_for_username;

  if (!form_on_page.only_for_fallback &&
      (form_on_page.HasPasswordElement() || form_on_page.IsSingleUsername())) {
    // Fill fields identifying information only for non-fallback case when
    // password element is found. In other cases a fill popup is shown on
    // clicking on each password field so no need in any field identifiers.
    result.username_element_renderer_id =
        form_on_page.username_element_renderer_id;
    result.username_may_use_prefilled_placeholder =
        form_on_page.username_may_use_prefilled_placeholder;

    result.password_element_renderer_id =
        form_on_page.password_element_renderer_id;
  }

  if (preferred_match.has_value()) {
    CHECK_EQ(PasswordForm::Scheme::kHtml, preferred_match.value().scheme);

    result.preferred_login.username_value =
        preferred_match.value().username_value;
    result.preferred_login.password_value =
        preferred_match.value().password_value;

    result.preferred_login.uses_account_store =
        preferred_match->IsUsingAccountStore();

    if (GetMatchType(preferred_match.value()) != GetLoginMatchType::kExact ||
        !IsSameOrigin(main_frame_origin, form_on_page.url)) {
      // If the origins of the |preferred_match|, the main frame and the form's
      // frame differ, then show the origin of the match.
      result.preferred_login.realm = GetPreferredRealm(preferred_match.value());
    }
  }

  // Add additional username/value pairs.
  for (const PasswordForm& match : matches) {
    if (preferred_match.has_value() &&
        (match.username_value == preferred_match.value().username_value &&
         match.password_value == preferred_match.value().password_value)) {
      continue;
    }
    PasswordAndMetadata value;
    value.username_value = match.username_value;
    value.password_value = match.password_value;
    value.uses_account_store = match.IsUsingAccountStore();

    if (GetMatchType(match) != GetLoginMatchType::kExact) {
      value.realm = GetPreferredRealm(match);
    } else if (!IsSameOrigin(main_frame_origin, match.url)) {
      // If the suggestion is for a cross-origin iframe, display the origin of
      // the suggestion.
      value.realm = GetPreferredRealm(match);
    }
    result.additional_logins.push_back(std::move(value));
  }

  result.suggestion_banned_fields = std::vector<autofill::FieldRendererId>(
      suggestion_banned_fields.begin(), suggestion_banned_fields.end());

  return result;
}

}  // namespace password_manager
