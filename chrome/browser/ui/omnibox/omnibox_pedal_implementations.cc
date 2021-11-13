// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/resources/grit/omnibox_pedal_synonyms.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"

// This carefully simplifies preprocessor condition usage below.
#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR))
#define SUPPORTS_DESKTOP_ICONS 1
#else
#define SUPPORTS_DESKTOP_ICONS 0
#endif

#if SUPPORTS_DESKTOP_ICONS
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_OPTIONAL_GOOGLE_CHROME
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_OPTIONAL_GOOGLE_CHROME,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_REQUIRED_DELETE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_REQUIRED_INFORMATION,
        },
#endif
    };
  }

  void Execute(ExecutionContext& context) const override {
    if (incognito_) {
      context.client_.OpenIncognitoClearBrowsingDataDialog();
    } else {
      OmniboxPedal::Execute(context);
    }
  }

  // This method override enables this Pedal to spoof its ID for metrics
  // reporting, making it possible to distinguish incognito usage.
  OmniboxPedalId GetMetricsId() const override {
    return incognito_ ? OmniboxPedalId::INCOGNITO_CLEAR_BROWSING_DATA : id();
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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_PASSWORDS_ONE_OPTIONAL_GOOGLE_CHROME
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_PASSWORDS_ONE_OPTIONAL_GOOGLE_CHROME,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_PASSWORDS_ONE_REQUIRED_MANAGER,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_PASSWORDS_ONE_REQUIRED_CREDENTIALS,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalManagePasswords() override = default;
};

// =============================================================================

class OmniboxPedalUpdateCreditCard : public OmniboxPedal {
 public:
  OmniboxPedalUpdateCreditCard()
      : OmniboxPedal(
            OmniboxPedalId::UPDATE_CREDIT_CARD,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT,
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD),
            GURL("chrome://settings/payments")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CREDIT_CARD_ONE_OPTIONAL_GOOGLE_CHROME
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CREDIT_CARD_ONE_OPTIONAL_GOOGLE_CHROME,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CREDIT_CARD_ONE_REQUIRED_CHANGE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CREDIT_CARD_ONE_REQUIRED_CREDIT_CARD_INFORMATION,
        },
#endif
    };
  }

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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_LAUNCH_INCOGNITO_ONE_OPTIONAL_GOOGLE_CHROME
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_LAUNCH_INCOGNITO_ONE_OPTIONAL_GOOGLE_CHROME,
        },
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_LAUNCH_INCOGNITO_ONE_OPTIONAL_CREATE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_LAUNCH_INCOGNITO_ONE_REQUIRED_INCOGNITO_WINDOW,
        },
#endif
    };
  }

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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_TRANSLATE_ONE_OPTIONAL_GOOGLE_CHROME
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_TRANSLATE_ONE_OPTIONAL_GOOGLE_CHROME,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_TRANSLATE_ONE_REQUIRED_CHANGE_LANGUAGE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_TRANSLATE_ONE_REQUIRED_THIS_PAGE,
        },
#endif
    };
  }

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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CHROME_ONE_REQUIRED_GOOGLE_CHROME
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CHROME_ONE_REQUIRED_GOOGLE_CHROME,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CHROME_ONE_REQUIRED_INSTALL,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalUpdateChrome() override = default;
};

// =============================================================================

class OmniboxPedalRunChromeSafetyCheck : public OmniboxPedal {
 public:
  OmniboxPedalRunChromeSafetyCheck()
      : OmniboxPedal(
            OmniboxPedalId::RUN_CHROME_SAFETY_CHECK,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_HINT,
                IDS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK),
            GURL()) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_RUN_CHROME_SAFETY_CHECK_ONE_OPTIONAL_ACTIVATE
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_RUN_CHROME_SAFETY_CHECK_ONE_OPTIONAL_ACTIVATE,
        },
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_RUN_CHROME_SAFETY_CHECK_ONE_OPTIONAL_GOOGLE_CHROME,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_RUN_CHROME_SAFETY_CHECK_ONE_REQUIRED_CHECKUP,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_RUN_CHROME_SAFETY_CHECK_ONE_REQUIRED_PASSWORDS,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalRunChromeSafetyCheck() override = default;
};

// =============================================================================

class OmniboxPedalManageSecuritySettings : public OmniboxPedal {
 public:
  OmniboxPedalManageSecuritySettings()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_SECURITY_SETTINGS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS),
            GURL()) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SECURITY_SETTINGS_ANY_OPTIONAL_GOOGLE_CHROME
        {
            false,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SECURITY_SETTINGS_ANY_OPTIONAL_GOOGLE_CHROME,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SECURITY_SETTINGS_ONE_REQUIRED_ENHANCED_PROTECTION,
        },
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SECURITY_SETTINGS_ONE_OPTIONAL_ALTER,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalManageSecuritySettings() override = default;
};

// =============================================================================

class OmniboxPedalManageCookies : public OmniboxPedal {
 public:
  OmniboxPedalManageCookies()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_COOKIES,
            LabelStrings(IDS_OMNIBOX_PEDAL_MANAGE_COOKIES_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_COOKIES_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_COOKIES_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_COOKIES),
            GURL()) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_COOKIES_ONE_REQUIRED_COOKIE_SETTINGS
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_COOKIES_ONE_REQUIRED_COOKIE_SETTINGS,
        },
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_COOKIES_ANY_REQUIRED_GOOGLE_CHROME,
        },
        {
            false,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_COOKIES_ANY_OPTIONAL_THIRD_PARTY,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalManageCookies() override = default;
};

// =============================================================================

class OmniboxPedalManageAddresses : public OmniboxPedal {
 public:
  OmniboxPedalManageAddresses()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_ADDRESSES,
            LabelStrings(IDS_OMNIBOX_PEDAL_MANAGE_ADDRESSES_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_ADDRESSES_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_ADDRESSES_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_ADDRESSES),
            GURL()) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_ADDRESSES_ONE_REQUIRED_CONTROL
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_ADDRESSES_ONE_REQUIRED_CONTROL,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_ADDRESSES_ONE_REQUIRED_SHIPPING_ADDRESSES,
        },
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_ADDRESSES_ONE_OPTIONAL_GOOGLE_CHROME,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalManageAddresses() override = default;
};

// =============================================================================

class OmniboxPedalManageSync : public OmniboxPedal {
 public:
  OmniboxPedalManageSync()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_SYNC,
            LabelStrings(IDS_OMNIBOX_PEDAL_MANAGE_SYNC_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_SYNC_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC),
            GURL()) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_ONE_REQUIRED_SYNC_SETTINGS
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_ONE_REQUIRED_SYNC_SETTINGS,
        },
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_ANY_REQUIRED_GOOGLE_CHROME,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalManageSync() override = default;
};

// =============================================================================

class OmniboxPedalManageSiteSettings : public OmniboxPedal {
 public:
  OmniboxPedalManageSiteSettings()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_SITE_SETTINGS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS),
            GURL()) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SITE_SETTINGS_ONE_REQUIRED_SITE_PERMISSIONS
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SITE_SETTINGS_ONE_REQUIRED_SITE_PERMISSIONS,
        },
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SITE_SETTINGS_ANY_REQUIRED_GOOGLE_CHROME,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalManageSiteSettings() override = default;
};

// =============================================================================

class OmniboxPedalAuthRequired : public OmniboxPedal {
 public:
  explicit OmniboxPedalAuthRequired(OmniboxPedalId id,
                                    LabelStrings label_strings)
      : OmniboxPedal(id, label_strings, GURL()) {}
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
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_DOC,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_DOC_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_DOC_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_DOC_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_DOC)) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveDocsIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_DOC_ONE_REQUIRED_GOOGLE_WORKSPACE
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_DOC_ONE_REQUIRED_GOOGLE_WORKSPACE,
        },
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_DOC_ANY_REQUIRED_CREATE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_DOC_ONE_REQUIRED_DOCUMENT,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalCreateGoogleDoc() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleSheet : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleSheet()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_SHEET,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SHEET_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SHEET_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SHEET_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SHEET)) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveSheetsIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SHEET_ANY_REQUIRED_CREATE
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SHEET_ANY_REQUIRED_CREATE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SHEET_ONE_REQUIRED_GOOGLE_WORKSPACE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SHEET_ONE_REQUIRED_SPREADSHEET,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalCreateGoogleSheet() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleSlide : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleSlide()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_SLIDE,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SLIDE_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SLIDE_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SLIDE_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SLIDE)) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveSlidesIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SLIDE_ONE_REQUIRED_CREATE
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SLIDE_ONE_REQUIRED_CREATE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SLIDE_ONE_REQUIRED_PRESENTATION,
        },
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SLIDE_ANY_REQUIRED_WORKSPACE,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalCreateGoogleSlide() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleCalendarEvent : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleCalendarEvent()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_CALENDAR_EVENT,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_CALENDAR_EVENT_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_CALENDAR_EVENT_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_CALENDAR_EVENT_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_CALENDAR_EVENT)) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleCalendarIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_CALENDAR_EVENT_ANY_REQUIRED_SCHEDULE
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_CALENDAR_EVENT_ANY_REQUIRED_SCHEDULE,
        },
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_CALENDAR_EVENT_ANY_REQUIRED_WORKSPACE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_CALENDAR_EVENT_ONE_REQUIRED_MEETING,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalCreateGoogleCalendarEvent() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleSite : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleSite()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_SITE,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SITE_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SITE_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SITE_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SITE)) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleSitesIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SITE_ANY_REQUIRED_CREATE
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SITE_ANY_REQUIRED_CREATE,
        },
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SITE_ANY_REQUIRED_WORKSPACE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SITE_ONE_REQUIRED_WEBSITE,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalCreateGoogleSite() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleKeepNote : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleKeepNote()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_KEEP_NOTE,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_KEEP_NOTE_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_KEEP_NOTE_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_KEEP_NOTE_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_KEEP_NOTE)) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleKeepNoteIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_KEEP_NOTE_ANY_REQUIRED_CREATE
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_KEEP_NOTE_ANY_REQUIRED_CREATE,
        },
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_KEEP_NOTE_ANY_REQUIRED_WORKSPACE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_KEEP_NOTE_ONE_REQUIRED_NOTES,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalCreateGoogleKeepNote() override = default;
};

// =============================================================================

class OmniboxPedalCreateGoogleForm : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleForm()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_FORM,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_FORM_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_FORM_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_FORM_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_FORM)) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveFormsIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_FORM_ANY_REQUIRED_CREATE
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_FORM_ANY_REQUIRED_CREATE,
        },
        {
            true,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_FORM_ANY_REQUIRED_WORKSPACE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_FORM_ONE_REQUIRED_SURVEY,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalCreateGoogleForm() override = default;
};

// =============================================================================

class OmniboxPedalSeeChromeTips : public OmniboxPedal {
 public:
  OmniboxPedalSeeChromeTips()
      : OmniboxPedal(
            OmniboxPedalId::SEE_CHROME_TIPS,
            LabelStrings(IDS_OMNIBOX_PEDAL_SEE_CHROME_TIPS_HINT,
                         IDS_OMNIBOX_PEDAL_SEE_CHROME_TIPS_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_SEE_CHROME_TIPS_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_SEE_CHROME_TIPS),
            GURL()) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_SEE_CHROME_TIPS_ONE_OPTIONAL_MAKE_THE_MOST_OF
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_SEE_CHROME_TIPS_ONE_OPTIONAL_MAKE_THE_MOST_OF,
        },
        {
            false,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_SEE_CHROME_TIPS_ANY_OPTIONAL_BROWSER,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_SEE_CHROME_TIPS_ONE_REQUIRED_NEW_CHROME_FEATURES,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalSeeChromeTips() override = default;
};

// =============================================================================

class OmniboxPedalManageGoogleAccount : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalManageGoogleAccount()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::MANAGE_GOOGLE_ACCOUNT,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_GOOGLE_ACCOUNT_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_GOOGLE_ACCOUNT_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_GOOGLE_ACCOUNT_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_GOOGLE_ACCOUNT)) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleSuperGIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_GOOGLE_ACCOUNT_ONE_REQUIRED_GOOGLE_ACCOUNT
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_GOOGLE_ACCOUNT_ONE_REQUIRED_GOOGLE_ACCOUNT,
        },
        {
            false,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_GOOGLE_ACCOUNT_ANY_OPTIONAL_BROWSER,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_GOOGLE_ACCOUNT_ONE_REQUIRED_CONTROL,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalManageGoogleAccount() override = default;
};

// =============================================================================

class OmniboxPedalChangeGooglePassword : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalChangeGooglePassword()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CHANGE_GOOGLE_PASSWORD,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CHANGE_GOOGLE_PASSWORD_HINT,
                IDS_OMNIBOX_PEDAL_CHANGE_GOOGLE_PASSWORD_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CHANGE_GOOGLE_PASSWORD_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CHANGE_GOOGLE_PASSWORD)) {}
#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleSuperGIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CHANGE_GOOGLE_PASSWORD_ONE_REQUIRED_GOOGLE_ACCOUNT_PASSWORD
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CHANGE_GOOGLE_PASSWORD_ONE_REQUIRED_GOOGLE_ACCOUNT_PASSWORD,
        },
        {
            false,
            false,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CHANGE_GOOGLE_PASSWORD_ANY_OPTIONAL_BROWSER,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CHANGE_GOOGLE_PASSWORD_ONE_REQUIRED_CHANGE,
        },
#endif
    };
  }

 protected:
  ~OmniboxPedalChangeGooglePassword() override = default;
};

// =============================================================================

class OmniboxPedalCloseIncognitoWindows : public OmniboxPedal {
 public:
  OmniboxPedalCloseIncognitoWindows()
      : OmniboxPedal(
            OmniboxPedalId::CLOSE_INCOGNITO_WINDOWS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CLOSE_INCOGNITO_WINDOWS_HINT,
                IDS_OMNIBOX_PEDAL_CLOSE_INCOGNITO_WINDOWS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CLOSE_INCOGNITO_WINDOWS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CLOSE_INCOGNITO_WINDOWS),
            GURL()) {}

#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kIncognitoIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_REQUIRED_CLOSE
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_REQUIRED_CLOSE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_REQUIRED_INCOGNITO,
        },
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_OPTIONAL_BROWSERS,
        },
#endif
    };
  }

  void Execute(ExecutionContext& context) const override {
    context.client_.CloseIncognitoWindows();
  }

 protected:
  ~OmniboxPedalCloseIncognitoWindows() override = default;
};

// =============================================================================

class OmniboxPedalPlayChromeDinoGame : public OmniboxPedal {
 public:
  OmniboxPedalPlayChromeDinoGame()
      : OmniboxPedal(
            OmniboxPedalId::PLAY_CHROME_DINO_GAME,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_PLAY_CHROME_DINO_GAME_HINT,
                IDS_OMNIBOX_PEDAL_PLAY_CHROME_DINO_GAME_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_PLAY_CHROME_DINO_GAME_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_PLAY_CHROME_DINO_GAME),
            GURL("chrome://dino")) {}

#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDinoIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups() const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_PLAY_CHROME_DINO_GAME_ONE_OPTIONAL_PLAY
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_PLAY_CHROME_DINO_GAME_ONE_OPTIONAL_PLAY,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_PLAY_CHROME_DINO_GAME_ONE_REQUIRED_GOOGLE_CHROME,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_PLAY_CHROME_DINO_GAME_ONE_REQUIRED_DINOSAUR,
        },
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_PLAY_CHROME_DINO_GAME_ONE_OPTIONAL_GAME,
        },
#endif
    };
  }

  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    return client.GetPrefs()->GetBoolean("allow_dinosaur_easter_egg");
  }

 protected:
  ~OmniboxPedalPlayChromeDinoGame() override = default;
};

// =============================================================================

class OmniboxPedalFindMyPhone : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalFindMyPhone()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::FIND_MY_PHONE,
            LabelStrings(IDS_OMNIBOX_PEDAL_FIND_MY_PHONE_HINT,
                         IDS_OMNIBOX_PEDAL_FIND_MY_PHONE_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_FIND_MY_PHONE_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_FIND_MY_PHONE)) {}

#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleSuperGIcon;
  }
#endif

 protected:
  ~OmniboxPedalFindMyPhone() override = default;
};

// =============================================================================

class OmniboxPedalManageGooglePrivacy : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalManageGooglePrivacy()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::MANAGE_GOOGLE_PRIVACY,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_GOOGLE_PRIVACY_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_GOOGLE_PRIVACY_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_GOOGLE_PRIVACY_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_GOOGLE_PRIVACY)) {}

#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kGoogleSuperGIcon;
  }
#endif

 protected:
  ~OmniboxPedalManageGooglePrivacy() override = default;
};

// =============================================================================

class OmniboxPedalManageChromeSettings : public OmniboxPedal {
 public:
  OmniboxPedalManageChromeSettings()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_CHROME_SETTINGS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_SETTINGS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_SETTINGS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_SETTINGS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_SETTINGS),
            GURL()) {}

 protected:
  ~OmniboxPedalManageChromeSettings() override = default;
};

// =============================================================================

class OmniboxPedalManageChromeDownloads : public OmniboxPedal {
 public:
  OmniboxPedalManageChromeDownloads()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_CHROME_DOWNLOADS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_DOWNLOADS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_DOWNLOADS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_DOWNLOADS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_DOWNLOADS),
            GURL()) {}

 protected:
  ~OmniboxPedalManageChromeDownloads() override = default;
};

// =============================================================================

class OmniboxPedalViewChromeHistory : public OmniboxPedal {
 public:
  OmniboxPedalViewChromeHistory()
      : OmniboxPedal(
            OmniboxPedalId::VIEW_CHROME_HISTORY,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_VIEW_CHROME_HISTORY_HINT,
                IDS_OMNIBOX_PEDAL_VIEW_CHROME_HISTORY_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_VIEW_CHROME_HISTORY_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_VIEW_CHROME_HISTORY),
            GURL()) {}

 protected:
  ~OmniboxPedalViewChromeHistory() override = default;
};

// =============================================================================

class OmniboxPedalShareThisPage : public OmniboxPedal {
 public:
  OmniboxPedalShareThisPage()
      : OmniboxPedal(
            OmniboxPedalId::SHARE_THIS_PAGE,
            LabelStrings(IDS_OMNIBOX_PEDAL_SHARE_THIS_PAGE_HINT,
                         IDS_OMNIBOX_PEDAL_SHARE_THIS_PAGE_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_SHARE_THIS_PAGE_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_SHARE_THIS_PAGE),
            GURL()) {}

#if SUPPORTS_DESKTOP_ICONS
  const gfx::VectorIcon& GetVectorIcon() const override {
    // Prefer the idiomatic icon for each platform. This icon selection
    // logic follows that of the sharing hub.
    // See: chrome/browser/ui/views/sharing_hub/sharing_hub_icon_view.cc
    // Note: When pedals are implemented on Android, we may want to
    // consider using omnibox::kShareIcon (three dots with lines).
    // TODO(orinj): Eliminate the code duplication here and get the
    // same icon from SharingHubIconView::GetVectorIcon once pedals
    // are moved to src-internal.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return omnibox::kShareIcon;
#elif defined(OS_MAC)
    return omnibox::kShareMacIcon;
#elif defined(OS_WIN)
    return omnibox::kShareWinIcon;
#else
    return omnibox::kSendIcon;
#endif
  }
#endif

  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    return client.IsSharingHubAvailable();
  }

  void Execute(ExecutionContext& context) const override {
    context.client_.OpenSharingHub();
  }

 protected:
  ~OmniboxPedalShareThisPage() override = default;
};

// =============================================================================

class OmniboxPedalManageChromeAccessibility : public OmniboxPedal {
 public:
  OmniboxPedalManageChromeAccessibility()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_CHROME_ACCESSIBILITY,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_ACCESSIBILITY_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_ACCESSIBILITY_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_ACCESSIBILITY_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_ACCESSIBILITY),
            GURL()) {}

 protected:
  ~OmniboxPedalManageChromeAccessibility() override = default;
};

// =============================================================================

class OmniboxPedalManageChromeOSAccessibility : public OmniboxPedal {
 public:
  OmniboxPedalManageChromeOSAccessibility()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_CHROMEOS_ACCESSIBILITY,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_CHROMEOS_ACCESSIBILITY_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_CHROMEOS_ACCESSIBILITY_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROMEOS_ACCESSIBILITY_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROMEOS_ACCESSIBILITY),
            GURL()) {}

  OmniboxPedalId GetMetricsId() const override {
    return OmniboxPedalId::MANAGE_CHROME_ACCESSIBILITY;
  }

 protected:
  ~OmniboxPedalManageChromeOSAccessibility() override = default;
};

// =============================================================================

class OmniboxPedalCustomizeChromeFonts : public OmniboxPedal {
 public:
  OmniboxPedalCustomizeChromeFonts()
      : OmniboxPedal(
            OmniboxPedalId::CUSTOMIZE_CHROME_FONTS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CUSTOMIZE_CHROME_FONTS_HINT,
                IDS_OMNIBOX_PEDAL_CUSTOMIZE_CHROME_FONTS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CUSTOMIZE_CHROME_FONTS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CUSTOMIZE_CHROME_FONTS),
            GURL()) {}

 protected:
  ~OmniboxPedalCustomizeChromeFonts() override = default;
};

// =============================================================================

class OmniboxPedalManageChromeThemes : public OmniboxPedal {
 public:
  OmniboxPedalManageChromeThemes()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_CHROME_THEMES,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_THEMES_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_THEMES_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_THEMES_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_THEMES),
            GURL()) {}

 protected:
  ~OmniboxPedalManageChromeThemes() override = default;
};

// =============================================================================

class OmniboxPedalCustomizeSearchEngines : public OmniboxPedal {
 public:
  OmniboxPedalCustomizeSearchEngines()
      : OmniboxPedal(
            OmniboxPedalId::CUSTOMIZE_SEARCH_ENGINES,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CUSTOMIZE_SEARCH_ENGINES_HINT,
                IDS_OMNIBOX_PEDAL_CUSTOMIZE_SEARCH_ENGINES_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CUSTOMIZE_SEARCH_ENGINES_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CUSTOMIZE_SEARCH_ENGINES),
            GURL()) {}

 protected:
  ~OmniboxPedalCustomizeSearchEngines() override = default;
};

// =============================================================================

std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>>
GetPedalImplementations(bool incognito, bool testing) {
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
  add(new OmniboxPedalRunChromeSafetyCheck());
  add(new OmniboxPedalManageSecuritySettings());
  add(new OmniboxPedalManageCookies());
  add(new OmniboxPedalManageAddresses());
  add(new OmniboxPedalManageSync());
  add(new OmniboxPedalManageSiteSettings());
  add(new OmniboxPedalSeeChromeTips());

  if (testing || BUILDFLAG(GOOGLE_CHROME_BRANDING)) {
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

  if (OmniboxFieldTrial::IsPedalsBatch3Enabled()) {
    if (incognito) {
      add(new OmniboxPedalCloseIncognitoWindows());
    }
    add(new OmniboxPedalPlayChromeDinoGame());
    add(new OmniboxPedalFindMyPhone());
    add(new OmniboxPedalManageGooglePrivacy());
    add(new OmniboxPedalManageChromeSettings());
    add(new OmniboxPedalManageChromeDownloads());
    add(new OmniboxPedalViewChromeHistory());
#if !defined(OS_CHROMEOS)
    // The sharing hub pedal is intentionally excluded
    // on ChromeOS because the sharing hub experience on that
    // platform is different from other desktop platforms.
    add(new OmniboxPedalShareThisPage());
    add(new OmniboxPedalManageChromeAccessibility());
#else
    add(new OmniboxPedalManageChromeOSAccessibility());
#endif
    add(new OmniboxPedalCustomizeChromeFonts());
    add(new OmniboxPedalManageChromeThemes());
    add(new OmniboxPedalCustomizeSearchEngines());
  }
  return pedals;
}
