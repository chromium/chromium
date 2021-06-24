// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_pedal_implementations.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/strings/grit/components_strings.h"

// This carefully simplifies preprocessor condition usage below.
#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#define SUPPORTS_DESKTOP_ICONS 1
#else
#define SUPPORTS_DESKTOP_ICONS 0
#endif

#if SUPPORTS_DESKTOP_ICONS
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

// =============================================================================

class OmniboxPedalClearBrowsingData : public OmniboxPedal {
 public:
  explicit OmniboxPedalClearBrowsingData(bool incognito)
      : OmniboxPedal(
            OmniboxPedalId::CLEAR_BROWSING_DATA,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA),
            GURL("chrome://settings/clearBrowserData")),
        incognito_(incognito) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() override {
    return {
        // TODO(orinj): Gather the fixed structures and reference l10n strings
        // once available (ideally in a new omnibox_pedal_synonyms.grdp file).
        // Here is an example of how this Pedal could be structured:
        // { true, true,
        // IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_GROUP_CLEAR_REQUIRED,
        // },
        // { true, true,
        // IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_GROUP_DATA_REQUIRED,
        // },
        // { false, true,
        // IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_GROUP_CHROME_OPTIONAL,
        // },
    };
  }

  void Execute(ExecutionContext& context) const override {
    if (incognito_) {
      context.client_.OpenIncognitoClearBrowsingDataDialog();
    } else {
      OmniboxPedal::Execute(context);
    }
  }

  // This method and the below overrides enable this Pedal to spoof its ID
  // for metrics reporting, making it possible to distinguish incognito usage.
  OmniboxPedalId GetMetricsId() const {
    return incognito_ ? OmniboxPedalId::INCOGNITO_CLEAR_BROWSING_DATA : id();
  }

  void RecordActionShown() const override {
    base::UmaHistogramEnumeration("Omnibox.PedalShown", GetMetricsId(),
                                  OmniboxPedalId::TOTAL_COUNT);
  }

  void RecordActionExecuted() const override {
    base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.Pedal",
                                  GetMetricsId(), OmniboxPedalId::TOTAL_COUNT);
  }

 protected:
  ~OmniboxPedalClearBrowsingData() override = default;
  bool incognito_;
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

 protected:
  ~OmniboxPedalManagePasswords() override = default;
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

 protected:
  ~OmniboxPedalUpdateCreditCard() override = default;
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

 protected:
  ~OmniboxPedalLaunchIncognito() override = default;
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

 protected:
  ~OmniboxPedalTranslate() override = default;
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

 protected:
  ~OmniboxPedalUpdateChrome() override = default;
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
            GURL()) {}

 protected:
  ~OmniboxPedalRunChromeSafetyCheck() override = default;
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
            GURL()) {}

 protected:
  ~OmniboxPedalManageSecuritySettings() override = default;
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
                     GURL()) {}

 protected:
  ~OmniboxPedalManageCookies() override = default;
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
                     GURL()) {}

 protected:
  ~OmniboxPedalManageAddresses() override = default;
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
                     GURL()) {}

 protected:
  ~OmniboxPedalManageSync() override = default;
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
            GURL()) {}

 protected:
  ~OmniboxPedalManageSiteSettings() override = default;
};

// =============================================================================

class OmniboxPedalAuthRequired : public OmniboxPedal {
 public:
  explicit OmniboxPedalAuthRequired(OmniboxPedalId id)
      : OmniboxPedal(id, OmniboxPedal::LabelStrings(), GURL()) {}
  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    return client.IsAuthenticated();
  }

 protected:
  ~OmniboxPedalAuthRequired() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleDoc : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleDoc()
      : OmniboxPedalAuthRequired(OmniboxPedalId::CREATE_GOOGLE_DOC) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveDocsIcon;
  }
#endif

 protected:
  ~OmniboxPedalCreateGoogleDoc() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleSheet : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleSheet()
      : OmniboxPedalAuthRequired(OmniboxPedalId::CREATE_GOOGLE_SHEET) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveSheetsIcon;
  }
#endif

 protected:
  ~OmniboxPedalCreateGoogleSheet() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleSlide : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleSlide()
      : OmniboxPedalAuthRequired(OmniboxPedalId::CREATE_GOOGLE_SLIDE) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveSlidesIcon;
  }
#endif

 protected:
  ~OmniboxPedalCreateGoogleSlide() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleCalendarEvent : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleCalendarEvent()
      : OmniboxPedalAuthRequired(OmniboxPedalId::CREATE_GOOGLE_CALENDAR_EVENT) {
  }
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleCalendarIcon;
  }
#endif

 protected:
  ~OmniboxPedalCreateGoogleCalendarEvent() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleSite : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleSite()
      : OmniboxPedalAuthRequired(OmniboxPedalId::CREATE_GOOGLE_SITE) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleSitesIcon;
  }
#endif

 protected:
  ~OmniboxPedalCreateGoogleSite() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleKeepNote : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleKeepNote()
      : OmniboxPedalAuthRequired(OmniboxPedalId::CREATE_GOOGLE_KEEP_NOTE) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleKeepNoteIcon;
  }
#endif

 protected:
  ~OmniboxPedalCreateGoogleKeepNote() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleForm : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleForm()
      : OmniboxPedalAuthRequired(OmniboxPedalId::CREATE_GOOGLE_FORM) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveFormsIcon;
  }
#endif

 protected:
  ~OmniboxPedalCreateGoogleForm() override = default;
};

// =============================================================================

class OmniboxPedalSeeChromeTips : public OmniboxPedal {
 public:
  OmniboxPedalSeeChromeTips()
      : OmniboxPedal(OmniboxPedalId::SEE_CHROME_TIPS,
                     OmniboxPedal::LabelStrings(),
                     GURL()) {}

 protected:
  ~OmniboxPedalSeeChromeTips() override = default;
};

// =============================================================================

class OmniboxPedalManageGoogleAccount : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalManageGoogleAccount()
      : OmniboxPedalAuthRequired(OmniboxPedalId::MANAGE_GOOGLE_ACCOUNT) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleSuperGIcon;
  }
#endif

 protected:
  ~OmniboxPedalManageGoogleAccount() override = default;
};

// =============================================================================

class OmniboxPedalChangeGooglePassword : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalChangeGooglePassword()
      : OmniboxPedalAuthRequired(OmniboxPedalId::CHANGE_GOOGLE_PASSWORD) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleSuperGIcon;
  }
#endif

 protected:
  ~OmniboxPedalChangeGooglePassword() override = default;
};

// =============================================================================

std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>>
GetPedalImplementations(bool with_branding, bool incognito) {
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(std::make_pair(pedal->id(), base::WrapRefCounted(pedal)));
  };

  add(new OmniboxPedalClearBrowsingData(incognito));
  add(new OmniboxPedalManagePasswords());
  add(new OmniboxPedalUpdateCreditCard());
  add(new OmniboxPedalLaunchIncognito());
  add(new OmniboxPedalTranslate());
  add(new OmniboxPedalUpdateChrome());
  if (OmniboxFieldTrial::IsPedalsBatch2Enabled()) {
    add(new OmniboxPedalRunChromeSafetyCheck());
    add(new OmniboxPedalManageSecuritySettings());
    add(new OmniboxPedalManageCookies());
    add(new OmniboxPedalManageAddresses());
    add(new OmniboxPedalManageSync());
    add(new OmniboxPedalManageSiteSettings());
    add(new OmniboxPedalSeeChromeTips());

    if (with_branding) {
      add(new OmniboxPedalCreateGoogleDoc());
      add(new OmniboxPedalCreateGoogleSheet());
      add(new OmniboxPedalCreateGoogleSlide());
      add(new OmniboxPedalCreateGoogleCalendarEvent());
      add(new OmniboxPedalCreateGoogleSite());
      add(new OmniboxPedalCreateGoogleKeepNote());
      add(new OmniboxPedalCreateGoogleForm());
      add(new OmniboxPedalManageGoogleAccount());
      add(new OmniboxPedalChangeGooglePassword());
    }
  }
  return pedals;
}
