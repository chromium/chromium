// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_implementations.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/strings/grit/components_strings.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

// =============================================================================

class OmniboxPedalClearBrowsingData : public OmniboxPedal {
 public:
  OmniboxPedalClearBrowsingData()
      : OmniboxPedal(
            OmniboxPedalId::CLEAR_BROWSING_DATA,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA),
            GURL("chrome://settings/clearBrowserData")) {}
};

// =============================================================================

class OmniboxPedalManagePasswords : public OmniboxPedal {
 public:
  OmniboxPedalManagePasswords()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_PASSWORDS,
            LabelStrings(IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_PASSWORDS_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_PASSWORDS),
            GURL("chrome://settings/passwords")) {}
};

// =============================================================================

class OmniboxPedalUpdateCreditCard : public OmniboxPedal {
 public:
  OmniboxPedalUpdateCreditCard()
      : OmniboxPedal(
            OmniboxPedalId::UPDATE_CREDIT_CARD,
            OmniboxPedal::LabelStrings(
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT,
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD),
            GURL("chrome://settings/payments")) {}
};

// =============================================================================

class OmniboxPedalLaunchIncognito : public OmniboxPedal {
 public:
  OmniboxPedalLaunchIncognito()
      : OmniboxPedal(
            OmniboxPedalId::LAUNCH_INCOGNITO,
            LabelStrings(IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT,
                         IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_LAUNCH_INCOGNITO),
            // Fake URL to distinguish matches.
            GURL("chrome://newtab?incognito=true")) {}

  void Execute(ExecutionContext& context) const override {
    context.client_.NewIncognitoWindow();
  }
  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    return client.IsIncognitoModeAvailable();
  }
};

// =============================================================================

class OmniboxPedalTranslate : public OmniboxPedal {
 public:
  OmniboxPedalTranslate()
      : OmniboxPedal(
            OmniboxPedalId::TRANSLATE,
            LabelStrings(IDS_OMNIBOX_PEDAL_TRANSLATE_HINT,
                         IDS_OMNIBOX_PEDAL_TRANSLATE_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_TRANSLATE_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_TRANSLATE),
            // Fake URL to distinguish matches.
            GURL("chrome://translate/pedals")) {}

  void Execute(ExecutionContext& context) const override {
    context.client_.PromptPageTranslation();
  }
  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    // Built-in chrome:// URLs do not generally support translation, and the
    // translate UI does not yet inform users with a clear helpful error message
    // when requesting translation for a page that doesn't support translation,
    // so this is a quick early-out to prevent bad message crashes.
    // See: https://crbug.com/1131136
    return !input.current_url().SchemeIs(
        client.GetEmbedderRepresentationOfAboutScheme());
  }
};

// =============================================================================

class OmniboxPedalUpdateChrome : public OmniboxPedal {
 public:
  OmniboxPedalUpdateChrome()
      : OmniboxPedal(
            OmniboxPedalId::UPDATE_CHROME,
            LabelStrings(IDS_OMNIBOX_PEDAL_UPDATE_CHROME_HINT,
                         IDS_OMNIBOX_PEDAL_UPDATE_CHROME_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_UPDATE_CHROME_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_UPDATE_CHROME),
            GURL("chrome://settings/help")) {}
};

// =============================================================================

class OmniboxPedalRunChromeSafetyCheck : public OmniboxPedal {
 public:
  OmniboxPedalRunChromeSafetyCheck()
      : OmniboxPedal(
            OmniboxPedalId::RUN_CHROME_SAFETY_CHECK,
            OmniboxPedal::LabelStrings(
                IDS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_HINT,
                IDS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK),
            GURL("chrome://settings/safetyCheck")) {}
};

// =============================================================================

class OmniboxPedalManageSecuritySettings : public OmniboxPedal {
 public:
  OmniboxPedalManageSecuritySettings()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_SECURITY_SETTINGS,
            OmniboxPedal::LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS),
            GURL("chrome://settings/security")) {}
};

// =============================================================================

class OmniboxPedalManageCookies : public OmniboxPedal {
 public:
  OmniboxPedalManageCookies()
      : OmniboxPedal(OmniboxPedalId::MANAGE_COOKIES,
                     OmniboxPedal::LabelStrings(
                         IDS_OMNIBOX_PEDAL_MANAGE_COOKIES_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_COOKIES_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_COOKIES_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_COOKIES),
                     GURL("chrome://settings/cookies")) {}
};

// =============================================================================

class OmniboxPedalManageAddresses : public OmniboxPedal {
 public:
  OmniboxPedalManageAddresses()
      : OmniboxPedal(OmniboxPedalId::MANAGE_ADDRESSES,
                     OmniboxPedal::LabelStrings(
                         IDS_OMNIBOX_PEDAL_MANAGE_ADDRESSES_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_ADDRESSES_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_ADDRESSES_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_ADDRESSES),
                     GURL("chrome://settings/addresses")) {}
};

// =============================================================================

class OmniboxPedalManageSync : public OmniboxPedal {
 public:
  OmniboxPedalManageSync()
      : OmniboxPedal(OmniboxPedalId::MANAGE_SYNC,
                     OmniboxPedal::LabelStrings(
                         IDS_OMNIBOX_PEDAL_MANAGE_SYNC_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_SYNC_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC),
                     GURL("chrome://settings/syncSetup/advanced")) {}
};

// =============================================================================

class OmniboxPedalManageSiteSettings : public OmniboxPedal {
 public:
  OmniboxPedalManageSiteSettings()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_SITE_SETTINGS,
            OmniboxPedal::LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS),
            GURL("chrome://settings/content")) {}
};

// =============================================================================

std::unordered_map<OmniboxPedalId, std::unique_ptr<OmniboxPedal>>
GetPedalImplementations() {
  std::unordered_map<OmniboxPedalId, std::unique_ptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedalId id, OmniboxPedal* pedal) {
    pedals.insert(std::make_pair(id, std::unique_ptr<OmniboxPedal>(pedal)));
  };
  add(OmniboxPedalId::CLEAR_BROWSING_DATA, new OmniboxPedalClearBrowsingData());
  add(OmniboxPedalId::MANAGE_PASSWORDS, new OmniboxPedalManagePasswords());
  add(OmniboxPedalId::UPDATE_CREDIT_CARD, new OmniboxPedalUpdateCreditCard());
  add(OmniboxPedalId::LAUNCH_INCOGNITO, new OmniboxPedalLaunchIncognito());
  add(OmniboxPedalId::TRANSLATE, new OmniboxPedalTranslate());
  add(OmniboxPedalId::UPDATE_CHROME, new OmniboxPedalUpdateChrome());
  if (OmniboxFieldTrial::IsPedalsBatch2Enabled()) {
    add(OmniboxPedalId::RUN_CHROME_SAFETY_CHECK,
        new OmniboxPedalRunChromeSafetyCheck());
    add(OmniboxPedalId::MANAGE_SECURITY_SETTINGS,
        new OmniboxPedalManageSecuritySettings());
    add(OmniboxPedalId::MANAGE_COOKIES, new OmniboxPedalManageCookies());
    add(OmniboxPedalId::MANAGE_ADDRESSES, new OmniboxPedalManageAddresses());
    add(OmniboxPedalId::MANAGE_SYNC, new OmniboxPedalManageSync());
    add(OmniboxPedalId::MANAGE_SITE_SETTINGS,
        new OmniboxPedalManageSiteSettings());
  }
  return pedals;
}
