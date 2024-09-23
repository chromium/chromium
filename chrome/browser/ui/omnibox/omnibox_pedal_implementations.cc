// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/resources/grit/omnibox_pedal_synonyms.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/app/vector_icons/vector_icons.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    // Note about translation strings: English preserves original separated
    // synonym group structure, but these strings are not translated (grit
    // message sets translateable=false). Non-English uses simplified whole
    // phrase trigger lists, and this string is translated.
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_REQUIRED_CLEAR_BROWSER_CACHE,
          },
      };
    }
  }

  void Execute(ExecutionContext& context) const override {
    if (incognito_) {
      context.client_->OpenIncognitoClearBrowsingDataDialog();
    } else {
      OmniboxPedal::Execute(context);
    }
  }

  // This method override enables this Pedal to spoof its ID for metrics
  // reporting, making it possible to distinguish incognito usage.
  OmniboxPedalId GetMetricsId() const override {
    return incognito_ ? OmniboxPedalId::INCOGNITO_CLEAR_BROWSING_DATA
                      : PedalId();
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
            GURL(chrome::kChromeUIPasswordManagerURL)) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_PASSWORDS_ONE_OPTIONAL_GOOGLE_CHROME,
          },
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_PASSWORDS_ONE_OPTIONAL_MANAGER,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_PASSWORDS_ONE_REQUIRED_PASSWORDS,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_PASSWORDS_ONE_REQUIRED_MANAGE_CHROME_PASSWORDS,
          },
      };
    }
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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CREDIT_CARD_ONE_REQUIRED_MANAGE_PAYMENT_METHODS,
          },
      };
    }
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
#if BUILDFLAG(IS_ANDROID)
            LabelStrings(
                IDS_ANDROID_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT,
                IDS_ANDROID_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUGGESTION_CONTENTS,
                IDS_ANDROID_ACC_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUFFIX,
                IDS_ANDROID_ACC_OMNIBOX_PEDAL_LAUNCH_INCOGNITO),
#else
            LabelStrings(IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT,
                         IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_LAUNCH_INCOGNITO),
#endif  // BUILDFLAG(IS_ANDROID)
        // Fake URL to distinguish matches.
            GURL("chrome://newtab?incognito=true")) {
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_LAUNCH_INCOGNITO_ONE_REQUIRED_ENTER_INCOGNITO_MODE,
          },
      };
    }
  }

  void Execute(ExecutionContext& context) const override {
    context.client_->NewIncognitoWindow();
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

#if !BUILDFLAG(IS_ANDROID)
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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_TRANSLATE_ONE_REQUIRED_TRANSLATE_THIS_PAGE,
          },
      };
    }
  }

  void Execute(ExecutionContext& context) const override {
    context.client_->PromptPageTranslation();
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
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CHROME_ONE_REQUIRED_UPDATE_CHROME,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalUpdateChrome() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

class OmniboxPedalRunChromeSafetyCheck : public OmniboxPedal {
 public:
  OmniboxPedalRunChromeSafetyCheck()
      : OmniboxPedal(
            OmniboxPedalId::RUN_CHROME_SAFETY_CHECK,
#if BUILDFLAG(IS_ANDROID)
            LabelStrings(
                IDS_ANDROID_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_HINT,
                IDS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK),
#else
            LabelStrings(
                IDS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_HINT,
                IDS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK),
#endif  // BUILDFLAG(IS_ANDROID)
            GURL("chrome://settings/safetyCheck?activateSafetyCheck")) {
    // If SafetyHub flag is enabled, the label strings and url should be
    // updated.
    if (base::FeatureList::IsEnabled(features::kSafetyHub)) {
      strings_ = LabelStrings(
          IDS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_V2_HINT,
          IDS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_V2_SUGGESTION_CONTENTS,
          IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_V2_SUFFIX,
          IDS_ACC_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_V2);
      url_ = GURL("chrome://settings/safetyCheck");
    }
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_RUN_CHROME_SAFETY_CHECK_ONE_REQUIRED_RUN_CHROME_SAFETY_CHECK,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalRunChromeSafetyCheck() override = default;
};

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
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
            GURL("chrome://settings/security")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SECURITY_SETTINGS_ONE_REQUIRED_MANAGE_SECURITY_SETTINGS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageSecuritySettings() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalManageCookies : public OmniboxPedal {
 public:
  OmniboxPedalManageCookies()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_COOKIES,
            LabelStrings(IDS_OMNIBOX_PEDAL_MANAGE_COOKIES_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_COOKIES_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_COOKIES_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_COOKIES),
            GURL("chrome://settings/cookies")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_COOKIES_ONE_REQUIRED_CHROME_COOKIE_SETTINGS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageCookies() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalManageAddresses : public OmniboxPedal {
 public:
  OmniboxPedalManageAddresses()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_ADDRESSES,
            LabelStrings(IDS_OMNIBOX_PEDAL_MANAGE_ADDRESSES_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_ADDRESSES_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_ADDRESSES_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_ADDRESSES),
            GURL("chrome://settings/addresses")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_ADDRESSES_ONE_REQUIRED_ADD_ADDRESS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageAddresses() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalManageSync : public OmniboxPedal {
 public:
  OmniboxPedalManageSync()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_SYNC,
            LabelStrings(IDS_OMNIBOX_PEDAL_MANAGE_SYNC_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_SYNC_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC),
            GURL("chrome://settings/syncSetup/advanced")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_ONE_REQUIRED_CHANGE_CHROME_BROWSER_SYNC_SETTINGS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageSync() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

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
            GURL(chrome::kChromeUIContentSettingsURL)) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SITE_SETTINGS_ONE_REQUIRED_ADJUST_SITE_PERMISSIONS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageSiteSettings() override = default;
};

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalAuthRequired : public OmniboxPedal {
 public:
  explicit OmniboxPedalAuthRequired(OmniboxPedalId id,
                                    LabelStrings label_strings,
                                    GURL url)
      : OmniboxPedal(id, label_strings, url) {}
  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    return client.IsAuthenticated();
  }

 protected:
  ~OmniboxPedalAuthRequired() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalCreateGoogleDoc : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleDoc()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_DOC,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_DOC_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_DOC_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_DOC_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_DOC),
            GURL("https://docs.google.com/document/u/0/"
                 "create?usp=chrome_actions")) {}

  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveDocsIcon;
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_DOC_ONE_REQUIRED_CREATE_GOOGLE_DOC,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCreateGoogleDoc() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalCreateGoogleSheet : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleSheet()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_SHEET,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SHEET_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SHEET_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SHEET_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SHEET),
            GURL("https://docs.google.com/spreadsheets/u/0/"
                 "create?usp=chrome_actions")) {}

  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveSheetsIcon;
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SHEET_ONE_REQUIRED_CREATE_GOOGLE_SHEET,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCreateGoogleSheet() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalCreateGoogleSlide : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleSlide()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_SLIDE,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SLIDE_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SLIDE_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SLIDE_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SLIDE),
            GURL("https://docs.google.com/presentation/u/0/"
                 "create?usp=chrome_actions")) {}

  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveSlidesIcon;
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SLIDE_ONE_REQUIRED_CREATE_GOOGLE_SLIDE,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCreateGoogleSlide() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalCreateGoogleCalendarEvent : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleCalendarEvent()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_CALENDAR_EVENT,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_CALENDAR_EVENT_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_CALENDAR_EVENT_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_CALENDAR_EVENT_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_CALENDAR_EVENT),
            GURL("https://calendar.google.com/calendar/u/0/r/"
                 "eventedit?usp=chrome_actions")) {}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& GetVectorIcon() const override {
    return vector_icons::kGoogleCalendarIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_CALENDAR_EVENT_ONE_REQUIRED_CREATE_GOOGLE_CAL_INVITE,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCreateGoogleCalendarEvent() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalCreateGoogleSite : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleSite()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_SITE,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SITE_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_SITE_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SITE_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_SITE),
            GURL("https://sites.google.com/u/0/create?usp=chrome_actions")) {}
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& GetVectorIcon() const override {
    return vector_icons::kGoogleSitesIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_SITE_ONE_REQUIRED_CREATE_GOOGLE_SITE,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCreateGoogleSite() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalCreateGoogleKeepNote : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleKeepNote()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_KEEP_NOTE,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_KEEP_NOTE_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_KEEP_NOTE_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_KEEP_NOTE_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_KEEP_NOTE),
            GURL("https://keep.google.com/u/0/?usp=chrome_actions#NEWNOTE")) {}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& GetVectorIcon() const override {
    return vector_icons::kGoogleKeepNoteIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_KEEP_NOTE_ONE_REQUIRED_CREATE_GOOGLE_KEEP_NOTE,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCreateGoogleKeepNote() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalCreateGoogleForm : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalCreateGoogleForm()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CREATE_GOOGLE_FORM,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_FORM_HINT,
                IDS_OMNIBOX_PEDAL_CREATE_GOOGLE_FORM_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_FORM_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CREATE_GOOGLE_FORM),
            GURL("https://docs.google.com/forms/u/0/"
                 "create?usp=chrome_actions")) {}

  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDriveFormsIcon;
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CREATE_GOOGLE_FORM_ONE_REQUIRED_CREATE_GOOGLE_FORM,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCreateGoogleForm() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalSeeChromeTips : public OmniboxPedal {
 public:
  OmniboxPedalSeeChromeTips()
      : OmniboxPedal(
            OmniboxPedalId::SEE_CHROME_TIPS,
            LabelStrings(IDS_OMNIBOX_PEDAL_SEE_CHROME_TIPS_HINT,
                         IDS_OMNIBOX_PEDAL_SEE_CHROME_TIPS_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_SEE_CHROME_TIPS_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_SEE_CHROME_TIPS),
            GURL("https://www.google.com/chrome/tips/")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SEE_CHROME_TIPS_ONE_REQUIRED_CHROME_FEATURES,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalSeeChromeTips() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalManageGoogleAccount : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalManageGoogleAccount()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::MANAGE_GOOGLE_ACCOUNT,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_GOOGLE_ACCOUNT_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_GOOGLE_ACCOUNT_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_GOOGLE_ACCOUNT_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_GOOGLE_ACCOUNT),
            GURL("https://myaccount.google.com/"
                 "?utm_source=ga-chrome-actions&utm_medium=manageGA")) {}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& GetVectorIcon() const override {
    return vector_icons::kGoogleGLogoMonochromeIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_GOOGLE_ACCOUNT_ONE_REQUIRED_CONTROL_MY_GOOGLE_ACCOUNT,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageGoogleAccount() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalChangeGooglePassword : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalChangeGooglePassword()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::CHANGE_GOOGLE_PASSWORD,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CHANGE_GOOGLE_PASSWORD_HINT,
                IDS_OMNIBOX_PEDAL_CHANGE_GOOGLE_PASSWORD_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CHANGE_GOOGLE_PASSWORD_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CHANGE_GOOGLE_PASSWORD),
            GURL("https://myaccount.google.com/signinoptions/"
                 "password?utm_source=ga-chrome-actions&utm_medium=changePW")) {
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& GetVectorIcon() const override {
    return vector_icons::kGoogleGLogoMonochromeIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
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
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CHANGE_GOOGLE_PASSWORD_ONE_REQUIRED_CHANGE_GMAIL_PASSWORD,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalChangeGooglePassword() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
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

  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kIncognitoCr2023Icon;
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_REQUIRED_DELETE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_REQUIRED_INCOGNITO_WINDOW,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_REQUIRED_CLOSE_INCOGNITO_WINDOW,
          },
      };
    }
  }

  void Execute(ExecutionContext& context) const override {
    context.client_->CloseIncognitoWindows();
  }

 protected:
  ~OmniboxPedalCloseIncognitoWindows() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

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

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kDinoCr2023Icon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_PLAY_CHROME_DINO_GAME_ONE_REQUIRED_PLAY_CHROME_DINO_GAME,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_PLAY_CHROME_DINO_GAME_ONE_REQUIRED_CHROME_DINO,
          },
      };
    }
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

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalFindMyPhone : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalFindMyPhone()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::FIND_MY_PHONE,
            LabelStrings(IDS_OMNIBOX_PEDAL_FIND_MY_PHONE_HINT,
                         IDS_OMNIBOX_PEDAL_FIND_MY_PHONE_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_FIND_MY_PHONE_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_FIND_MY_PHONE),
            GURL("https://myaccount.google.com/"
                 "find-your-phone?utm_source=ga-chrome-actions&utm_medium="
                 "findYourPhone")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_FIND_MY_PHONE_ONE_REQUIRED_HELP_ME_LOCATE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_FIND_MY_PHONE_ONE_REQUIRED_LOST_DEVICE,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_FIND_MY_PHONE_ONE_REQUIRED_FIND_LOST_PHONE,
          },
      };
    }
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& GetVectorIcon() const override {
    return vector_icons::kGoogleGLogoMonochromeIcon;
  }
#endif

 protected:
  ~OmniboxPedalFindMyPhone() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalManageGooglePrivacy : public OmniboxPedalAuthRequired {
 public:
  OmniboxPedalManageGooglePrivacy()
      : OmniboxPedalAuthRequired(
            OmniboxPedalId::MANAGE_GOOGLE_PRIVACY,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_GOOGLE_PRIVACY_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_GOOGLE_PRIVACY_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_GOOGLE_PRIVACY_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_GOOGLE_PRIVACY),
            GURL("https://myaccount.google.com/"
                 "data-and-privacy?utm_source=ga-chrome-actions&utm_medium="
                 "managePrivacy")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_GOOGLE_PRIVACY_ONE_REQUIRED_MANAGE_GOOGLE_PRIVACY_SETTINGS,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_GOOGLE_PRIVACY_ONE_REQUIRED_CHANGE_GOOGLE_PRIVACY_SETTINGS,
          },
      };
    }
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& GetVectorIcon() const override {
    return vector_icons::kGoogleGLogoMonochromeIcon;
  }
#endif

 protected:
  ~OmniboxPedalManageGooglePrivacy() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

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
            GURL("chrome://settings")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_SETTINGS_ONE_OPTIONAL_CONTROL,
          },
          {true, true,
           IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_SETTINGS_ONE_REQUIRED_CHROME_BROWSER_SETTINGS},
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_SETTINGS_ONE_REQUIRED_CHANGE_CHROME_SETTINGS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageChromeSettings() override = default;
};

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
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
            GURL("chrome://downloads")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_DOWNLOADS_ONE_REQUIRED_CONTROL,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_DOWNLOADS_ONE_REQUIRED_CHROME_BROWSER_DOWNLOADS,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_DOWNLOADS_ONE_REQUIRED_MANAGE_CHROME_DOWNLOADS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageChromeDownloads() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

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
            GURL("chrome://history")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_VIEW_CHROME_HISTORY_ONE_REQUIRED_REVISIT,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_VIEW_CHROME_HISTORY_ONE_REQUIRED_GOOGLE_CHROME_BROWSING_HISTORY,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_VIEW_CHROME_HISTORY_ONE_REQUIRED_SEE_CHROME_HISTORY,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalViewChromeHistory() override = default;
};

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
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

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SHARE_THIS_PAGE_ONE_REQUIRED_SHARE_LINK_WITH_QR_CODE,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SHARE_THIS_PAGE_ONE_REQUIRED_SHARE_THIS_PAGE,
          },
      };
    }
  }

  const gfx::VectorIcon& GetVectorIcon() const override {
    return GetSharingHubVectorIcon();
  }

  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    return client.IsSharingHubAvailable();
  }

  void Execute(ExecutionContext& context) const override {
    context.client_->OpenSharingHub();
  }

 protected:
  ~OmniboxPedalShareThisPage() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

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
            GURL("chrome://settings/accessibility")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_ACCESSIBILITY_ONE_OPTIONAL_CUSTOMIZE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_ACCESSIBILITY_ONE_REQUIRED_ACCESSIBILITY_SETTINGS,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_ACCESSIBILITY_ONE_REQUIRED_CUSTOMIZE_CHROME_ACCESSIBILITY,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageChromeAccessibility() override = default;
};

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
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
            GURL("chrome://os-settings/osAccessibility")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROMEOS_ACCESSIBILITY_ONE_OPTIONAL_CUSTOMIZE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROMEOS_ACCESSIBILITY_ONE_REQUIRED_ACCESSIBILITY_SETTINGS,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROMEOS_ACCESSIBILITY_ONE_REQUIRED_CUSTOMIZE_CHROMEOS_ACCESSIBILITY,
          },
      };
    }
  }

  OmniboxPedalId GetMetricsId() const override {
    return OmniboxPedalId::MANAGE_CHROME_ACCESSIBILITY;
  }

 protected:
  ~OmniboxPedalManageChromeOSAccessibility() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
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
            GURL("chrome://settings/fonts")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_CHROME_FONTS_ONE_OPTIONAL_CUSTOMIZE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_CHROME_FONTS_ONE_REQUIRED_GOOGLE_CHROME,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_CHROME_FONTS_ONE_REQUIRED_FONT_SIZING,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_CHROME_FONTS_ONE_REQUIRED_CHANGE_BROWSER_FONT,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCustomizeChromeFonts() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
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
            GURL("chrome://new-tab-page/?customize=backgrounds")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_THEMES_ONE_OPTIONAL_CUSTOMIZE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_THEMES_ONE_REQUIRED_CHROME_BACKGROUNDS,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_THEMES_ONE_REQUIRED_CUSTOMIZE_CHROME_APPEARANCE,
          },
      };
    }
  }

  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    // The URL for this pedal is specific to Google/Chrome. Avoid confusion
    // with user's default engine & new tab page when another is selected.
    return search::DefaultSearchProviderIsGoogle(
        client.GetTemplateURLService());
  }

 protected:
  ~OmniboxPedalManageChromeThemes() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
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
            GURL("chrome://settings/searchEngines")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_SEARCH_ENGINES_ONE_OPTIONAL_CUSTOMIZE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_SEARCH_ENGINES_ONE_REQUIRED_CUSTOM_SEARCH_ENGINES,
          },
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_SEARCH_ENGINES_ONE_OPTIONAL_GOOGLE_CHROME,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_SEARCH_ENGINES_ONE_REQUIRED_ADD_CUSTOM_SEARCH,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCustomizeSearchEngines() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

#if !BUILDFLAG(IS_ANDROID)
class OmniboxPedalSetChromeAsDefaultBrowser : public OmniboxPedal {
 public:
  OmniboxPedalSetChromeAsDefaultBrowser()
      : OmniboxPedal(
            OmniboxPedalId::SET_CHROME_AS_DEFAULT_BROWSER,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_HINT,
                IDS_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER),
            GURL("chrome://settings/defaultBrowser")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_REQUIRED_HOW_TO_MAKE_CHROME_MY_DEFAULT_BROWSER,
          },
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_OPTIONAL_SELECT,
          },
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_OPTIONAL_DEFAULT_BROWSER,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_REQUIRED_ALWAYS_OPEN_LINKS_IN_CHROME,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalSetChromeAsDefaultBrowser() override = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// =============================================================================

const gfx::VectorIcon& GetSharingHubVectorIcon() {
#if BUILDFLAG(IS_MAC)
  return omnibox::kShareMacChromeRefreshIcon;
#elif BUILDFLAG(IS_WIN)
  return omnibox::kShareWinChromeRefreshIcon;
#elif BUILDFLAG(IS_LINUX)
  return omnibox::kShareLinuxChromeRefreshIcon;
#else
  return omnibox::kShareChromeRefreshIcon;
#endif
}

// NOTE: When `testing` is true, all platform-appropriate pedals should be
// instantiated so that realbox icon checks can detect missing icons for
// pedals that may or may not be instantiated according to flag states.
std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>>
GetPedalImplementations(bool incognito, bool guest, bool testing) {
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    const bool inserted =
        pedals
            .insert(
                std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)))
            .second;
    DCHECK(inserted);
  };

#if BUILDFLAG(IS_ANDROID)
  if (!incognito && !guest) {
    add(new OmniboxPedalClearBrowsingData(/*incognito=*/false));
  }
  add(new OmniboxPedalManagePasswords());
  add(new OmniboxPedalUpdateCreditCard());
  add(new OmniboxPedalLaunchIncognito());
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    add(new OmniboxPedalRunChromeSafetyCheck());
  }
  add(new OmniboxPedalPlayChromeDinoGame());
  add(new OmniboxPedalManageSiteSettings());
  add(new OmniboxPedalManageChromeSettings());
  add(new OmniboxPedalViewChromeHistory());
  add(new OmniboxPedalManageChromeAccessibility());
#else  // BUILDFLAG(IS_ANDROID)
  // Clear Browsing Data functionality is disabled in guest mode, so
  // the pedal for accessing it should not be included.
  if (!guest) {
    add(new OmniboxPedalClearBrowsingData(incognito));
  }
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

  if (incognito) {
    add(new OmniboxPedalCloseIncognitoWindows());
  }
  add(new OmniboxPedalPlayChromeDinoGame());
  add(new OmniboxPedalFindMyPhone());
  add(new OmniboxPedalManageGooglePrivacy());
  add(new OmniboxPedalManageChromeSettings());
  add(new OmniboxPedalManageChromeDownloads());
  add(new OmniboxPedalViewChromeHistory());
#if !BUILDFLAG(IS_CHROMEOS)
  // The sharing hub pedal is intentionally excluded
  // on ChromeOS because the sharing hub experience on that
  // platform is different from other desktop platforms.
  add(new OmniboxPedalShareThisPage());
  add(new OmniboxPedalManageChromeAccessibility());
  add(new OmniboxPedalSetChromeAsDefaultBrowser());
#else   // !BUILDFLAG(IS_CHROMEOS)
  add(new OmniboxPedalManageChromeOSAccessibility());
#endif  // !BUILDFLAG(IS_CHROMEOS)
  add(new OmniboxPedalCustomizeChromeFonts());
  add(new OmniboxPedalManageChromeThemes());
  add(new OmniboxPedalCustomizeSearchEngines());
#endif  // BUILDFLAG(IS_ANDROID)

  return pedals;
}
