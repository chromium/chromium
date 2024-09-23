// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"

namespace privacy_sandbox {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1ConsentDecisionMade,
                                false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1EEANoticeAcknowledged,
                                false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1RowNoticeAcknowledged,
                                false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged, false);
  registry->RegisterIntegerPref(prefs::kPrivacySandboxM1PromptSuppressed, 0);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1TopicsEnabled, false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1FledgeEnabled, false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1Restricted, false);

  registry->RegisterTimePref(prefs::kPrivacySandboxTopicsDataAccessibleSince,
                             base::Time());
  registry->RegisterListPref(prefs::kPrivacySandboxBlockedTopics);
  registry->RegisterDictionaryPref(prefs::kPrivacySandboxFledgeJoinBlocked);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxNoticeDisplayed, false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxConsentDecisionMade,
                                false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxNoConfirmationSandboxDisabled, false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxNoConfirmationSandboxRestricted, false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxNoConfirmationSandboxManaged, false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked, false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxNoConfirmationManuallyControlled, false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxDisabledInsufficientConfirmation, false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized, false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(prefs::kPrivacySandboxTopicsConsentGiven,
                                false);
  registry->RegisterTimePref(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                             base::Time());
  registry->RegisterIntegerPref(
      prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
      static_cast<int>(TopicsConsentUpdateSource::kDefaultValue));
  registry->RegisterStringPref(
      prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate, "");
  registry->RegisterTimePref(prefs::kPrivacySandboxSentimentSurveyLastSeen,
                             base::Time());
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterListPref(prefs::kPrivacySandboxActivityTypeRecord2);
#endif
  // Register prefs for tracking protection.
  tracking_protection::RegisterProfilePrefs(registry);

  // Register prefs for the privacy sandbox notice storage system.
  PrivacySandboxNoticeStorage::RegisterProfilePrefs(registry);
}

}  // namespace privacy_sandbox
