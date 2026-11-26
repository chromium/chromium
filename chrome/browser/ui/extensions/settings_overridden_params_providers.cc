// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/extension_url_overrides.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resources_util.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data_resolver_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/search_override_stack.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engine_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_url_handler.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_set.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace settings_overridden_params {

namespace {

// Returns true if the given |template_url| corresponds to Google search.
bool IsGoogleSearch(const TemplateURL& template_url,
                    const TemplateURLService& template_url_service) {
  GURL search_url =
      template_url.GenerateSearchURL(template_url_service.search_terms_data());
  return google_util::IsGoogleSearchUrl(search_url);
}

// Returns true if Google is the default search provider.
bool GoogleIsDefaultSearchProvider(Profile* profile) {
  const TemplateURLService* const template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* const default_search =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search) {
    // According to TemplateURLService, |default_search| can be null if the
    // default search engine is disabled by policy.
    return false;
  }

  return IsGoogleSearch(*default_search, *template_url_service);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Formats a search URL for display in the explicit choice dialog.
std::u16string FormatSearchUrl(const GURL& search_url) {
  return url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
      search_url);
}

// Helper to find the matching built-in prepopulated engine for a search URL,
// if any, by matching the URL's domain against known prepopulated engines.
const TemplateURLPrepopulateData::PrepopulatedEngine*
GetPrepopulatedEngineForSearchUrl(const GURL& search_url, Profile& profile) {
  if (!search_url.is_valid()) {
    return nullptr;
  }

  // First, look up the search engine type by matching the URL's domain against
  // known prepopulated engines (including alternate URLs and migrated engines).
  const SearchEngineType engine_type =
      search_engine_utils::GetEngineType(search_url);
  if (engine_type == SEARCH_ENGINE_OTHER) {
    return nullptr;
  }

  // From the engine type, find the most appropriate prepopulated entry. Use the
  // prepopulate system for this, rather than attempting a bespoke domain-match,
  // for consistency and maintainability.
  return CHECK_DEREF(TemplateURLPrepopulateData::ResolverFactory::GetForProfile(
                         &profile))
      .GetEngineFromFullList(engine_type);
}

// Looks up the bundled theme resource ID for a built-in search engine icon,
// without decoding image bytes.
std::optional<int> GetBuiltinSearchEngineIconResourceId(
    const TemplateURL& template_url) {
  if (!template_url.GetBaseBuiltinResourceId().has_value()) {
    return std::nullopt;
  }
  const int resource_id = ResourcesUtil::GetThemeResourceId(
      template_url.GetBuiltinImageResourceId());
  CHECK_NE(resource_id, -1);
  return resource_id;
}

std::optional<int> GetBuiltinSearchEngineIconResourceId(
    const TemplateURLPrepopulateData::PrepopulatedEngine& engine) {
  std::optional<std::string> image_resource_id =
      TemplateURL::GetBuiltinImageResourceId(engine);
  if (!image_resource_id.has_value()) {
    return std::nullopt;
  }
  const int resource_id = ResourcesUtil::GetThemeResourceId(*image_resource_id);
  CHECK_NE(resource_id, -1);
  return resource_id;
}

// Builds a lightweight icon descriptor from a built-in resource ID or remote
// favicon URL.
SettingsOverriddenDialogController::SettingOption::IconSource MakeIconSource(
    std::optional<int> builtin_icon_resource_id,
    const GURL& favicon_url) {
  if (builtin_icon_resource_id.has_value()) {
    return *builtin_icon_resource_id;
  }
  if (favicon_url.is_valid() && favicon_url.SchemeIsHTTPOrHTTPS()) {
    return favicon_url;
  }
  return std::monostate{};
}

// Returns the number of extensions that are currently enabled that override the
// default search setting.
size_t GetNumberOfExtensionsThatOverrideSearch(Profile& profile) {
  const auto* const registry = extensions::ExtensionRegistry::Get(&profile);
  const auto overrides_search = [](auto extension) {
    auto* const settings = extensions::SettingsOverrides::Get(extension.get());
    return settings && settings->search_engine;
  };
  return std::ranges::count_if(registry->enabled_extensions(),
                               overrides_search);
}

struct SecondarySearchInfo {
  enum class Type {
    // Google is the secondary search engine.
    kGoogle,
    // The secondary search is one of the default-populated searches, but is
    // not Google.
    kNonGoogleInDefaultList,
    // Some other search engine is the secondary search.
    kOther,
  };

  Type type;

  // The origin of the search engine. Only populated if the secondary search
  // is not from another extension.
  GURL origin;

  // The name of the search engine, if available.
  std::u16string name;

  // Theme resource ID for a bundled search engine icon, if available.
  std::optional<int> builtin_icon_resource_id;

  // The favicon URL to fetch over the network when no built-in icon is
  // available.
  GURL favicon_url;
};

// Returns details about the search that would take over, if the currently-
// controlling extension were to be disabled.
SecondarySearchInfo GetSecondarySearchInfo(Profile& profile) {
  // First, check if there's another extension that would take over.
  size_t num_overriding_extensions =
      GetNumberOfExtensionsThatOverrideSearch(profile);
  // This method should only be called when there's an extension that overrides
  // the search engine.
  CHECK_GE(num_overriding_extensions, 1u, base::NotFatalUntil::M162);

  // Another extension would take over.
  std::optional<const TemplateURL> secondary_extension_search;
  if (num_overriding_extensions > 1) {
    const std::string& search_pref_key =
        DefaultSearchManager::kDefaultSearchProviderDataPrefName;

    ExtensionPrefValueMap* extension_prefs_value_map =
        ExtensionPrefValueMapFactory::GetForBrowserContext(&profile);

    std::string primary_ext_id =
        extension_prefs_value_map->GetExtensionControllingPref(search_pref_key);

    // Search overrides are registered in the regular profile
    // (TemplateURLService is redirected to the original profile in OTR
    // profiles), so always query the regular profile's effective pref value.
    const base::Value* secondary_pref =
        extension_prefs_value_map->GetEffectivePrefValue(
            search_pref_key, /*incognito=*/false,
            /*from_incognito=*/nullptr,
            /*ignore_extension_id=*/primary_ext_id);

    if (secondary_pref) {
      std::unique_ptr<TemplateURLData> url_data =
          TemplateURLDataFromDictionary(secondary_pref->GetDict());
      if (url_data) {
        secondary_extension_search.emplace(std::move(*url_data));
      }
    }
  }

  TemplateURLService* const template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  const TemplateURL* const secondary_search =
      secondary_extension_search
          ? &(*secondary_extension_search)
          : template_url_service->GetDefaultSearchProviderIgnoringExtensions();

  if (!secondary_search) {
    // We couldn't find a default (this could potentially happen if e.g. the
    // default search engine is disabled by policy).
    // TODO(devlin): It *seems* like in that case, extensions also shouldn't be
    // able to override it. Investigate.
    return {SecondarySearchInfo::Type::kOther};
  }

  const GURL search_url = secondary_search->GenerateSearchURL(
      template_url_service->search_terms_data());
  SecondarySearchInfo search_info;
  search_info.origin = search_url.DeprecatedGetOriginAsURL();

  if (base::FeatureList::IsEnabled(
          extensions_features::kSearchEngineExplicitChoiceDialog)) {
    if (secondary_extension_search) {
      // Display the known name and icon for the secondary extension's
      // search, if it is known by the browser. Otherwise, show the formatted
      // search URL and supply no favicon (downstream code can use a fallback
      // icon).
      if (const auto* prepopulated_engine =
              GetPrepopulatedEngineForSearchUrl(search_url, profile)) {
        search_info.name = std::u16string(prepopulated_engine->name);
        search_info.builtin_icon_resource_id =
            GetBuiltinSearchEngineIconResourceId(*prepopulated_engine);
        if (!search_info.builtin_icon_resource_id) {
          search_info.favicon_url = GURL(prepopulated_engine->favicon_url);
        }
      } else {
        search_info.name = FormatSearchUrl(search_url);
        search_info.favicon_url = GURL();
      }
    } else {
      search_info.name = secondary_search->short_name();
      search_info.builtin_icon_resource_id =
          GetBuiltinSearchEngineIconResourceId(*secondary_search);
      if (!search_info.builtin_icon_resource_id) {
        search_info.favicon_url = secondary_search->favicon_url();
      }
    }
  } else {
    search_info.name = secondary_search->short_name();
  }
  if (google_util::IsGoogleSearchUrl(search_url)) {
    search_info.type = SecondarySearchInfo::Type::kGoogle;
  } else if (template_url_service->ShowInDefaultList(secondary_search)) {
    search_info.type = SecondarySearchInfo::Type::kNonGoogleInDefaultList;
  } else {
    search_info.type = SecondarySearchInfo::Type::kOther;
  }

  return search_info;
}

// The explicit choice dialog requires names for the options. One prior bug
// yielded a case where no name was returned for an option. Instrument this
// to detect whether we ever show users a dialog with missing information.
void LogMissingParams(const ExtensionSettingsOverriddenDialog::Params& params) {
  MissingParams param = MissingParams::kNone;
  if (!params.content.new_setting || params.content.new_setting->text.empty()) {
    param = MissingParams::kMissingNewSearchName;
  } else if (!params.content.previous_setting ||
             params.content.previous_setting->text.empty()) {
    param = MissingParams::kMissingPreviousSearchName;
  }
  base::UmaHistogramEnumeration("Extensions.SettingsOverridden.MissingParams",
                                param);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace

std::optional<ExtensionSettingsOverriddenDialog::Params> GetNtpOverriddenParams(
    Profile* profile) {
  const GURL& ntp_url = chrome::ChromeUINewTabURLAsGURL();
  const extensions::Extension* extension =
      ExtensionUrlOverrides::GetExtensionControllingURL(ntp_url, profile);
  if (!extension) {
    return std::nullopt;
  }

  // This preference tracks whether users have acknowledged the extension's
  // control, so that they are not warned twice about the same extension.
  const char* preference_name = extensions::kNtpOverridingExtensionAcknowledged;

  std::vector<GURL> possible_rewrites =
      content::BrowserURLHandler::GetInstance()->GetPossibleRewrites(ntp_url,
                                                                     profile);
  // We already know that the extension is the primary NTP controller.
  CHECK(!possible_rewrites.empty(), base::NotFatalUntil::M162);
  DCHECK_EQ(extension->url().host(), possible_rewrites[0].host())
      << "Unexpected NTP URL: " << possible_rewrites[0];

  // Find whether the default NTP would take over if the extension were to be
  // removed. This might not be the case if, e.g. an enterprise policy set the
  // NTP or the default search provided its own.
  bool default_ntp_is_secondary = true;
  if (possible_rewrites.size() > 1) {
    default_ntp_is_secondary =
        possible_rewrites[1] == ntp_url ||
        possible_rewrites[1] == chrome::ChromeUINewTabPageURLAsGURL() ||
        possible_rewrites[1] == GURL(chrome::kChromeUINewTabPageThirdPartyURL);
  }
  // Check if there's another extension that would take over (this isn't
  // included in BrowserURLHandler::GetPossibleRewrites(), which only takes the
  // highest-priority from each source).
  default_ntp_is_secondary &=
      ExtensionUrlOverrides::GetNumberOfExtensionsOverridingURL(ntp_url,
                                                                profile) == 1;

  // We show different dialogs based on whether the NTP would return to the
  // default Chrome NTP with Google search.
  bool use_back_to_google_messaging =
      default_ntp_is_secondary && GoogleIsDefaultSearchProvider(profile);

  constexpr char kGenericDialogHistogramName[] =
      "Extensions.SettingsOverridden.GenericNtpOverriddenDialogResult";
  constexpr char kBackToGoogleDialogHistogramName[] =
      "Extensions.SettingsOverridden.BackToGoogleNtpOverriddenDialogResult";

  std::u16string dialog_title;
  const char* histogram_name = nullptr;
  const gfx::VectorIcon* icon = nullptr;
  if (use_back_to_google_messaging) {
    dialog_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_NTP_OVERRIDDEN_DIALOG_TITLE_BACK_TO_GOOGLE);
    histogram_name = kBackToGoogleDialogHistogramName;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    icon = &vector_icons::kGoogleGLogoIcon;
#endif
  } else {
    dialog_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_NTP_OVERRIDDEN_DIALOG_TITLE_GENERIC);
    histogram_name = kGenericDialogHistogramName;
  }
  CHECK(!dialog_title.empty(), base::NotFatalUntil::M162);
  CHECK(histogram_name, base::NotFatalUntil::M162);

  std::u16string dialog_message = l10n_util::GetStringFUTF16(
      IDS_EXTENSION_NTP_OVERRIDDEN_DIALOG_BODY_GENERIC,
      extensions::ui_util::GetFixupExtensionNameForUIDisplay(
          extension->name()));

  SettingsOverriddenDialogController::ShowParams show_params(
      std::move(dialog_title), std::move(dialog_message), icon);
  return ExtensionSettingsOverriddenDialog::Params(
      extension->id(), extension->name(), preference_name, histogram_name,
      std::move(show_params));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
bool ExtensionSearchOverrideMatchesExistingEngine(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!extensions::GetExtensionOverridingSearchEngine(profile)) {
    return false;
  }

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  CHECK(template_url_service);
  if (!template_url_service->IsExtensionControlledDefaultSearch()) {
    return false;
  }

  const TemplateURL* default_search =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search) {
    return false;
  }

  const GURL search_url(default_search->url());
  if (!search_url.is_valid()) {
    return false;
  }

  // NOTE: Normally, we wouldn't want to use direct equality comparison of
  // GURL::GetOrigin() because of edge cases like inner URLs with filesystem,
  // etc. This is okay here, because if the origins don't match we fall through
  // to showing the dialog. That's likely good if any extension is doing
  // something as crazy as using filesystem: URLs as a search engine.
  const SecondarySearchInfo secondary_search = GetSecondarySearchInfo(*profile);
  return !secondary_search.origin.is_empty() &&
         secondary_search.origin == search_url.DeprecatedGetOriginAsURL();
}

bool HasUnacknowledgedMatchingDseExtension(Profile* profile) {
  if (!profile) {
    return false;
  }

  const extensions::Extension* extension =
      extensions::GetExtensionOverridingSearchEngine(profile);
  if (!extension) {
    return false;
  }

  if (!ExtensionSearchOverrideMatchesExistingEngine(profile)) {
    return false;
  }

  return !ExtensionSettingsOverriddenDialog::HasAcknowledgedExtension(
      *profile, extension->id(),
      ControlledHomeDialogController::kAcknowledgedPreference);
}

std::optional<ExtensionSettingsOverriddenDialog::Params>
GetSearchOverriddenParams(Profile& profile) {
  const extensions::Extension* extension =
      extensions::GetExtensionOverridingSearchEngine(&profile);
  if (!extension) {
    return std::nullopt;
  }

  // Find the active search engine. This is normally the one provided by
  // `extension`, but GetExtensionOverridingSearchEngine() only consults
  // extension prefs, while a default search engine set (or disabled) by
  // mandatory enterprise policy takes precedence over the extension's. In that
  // case the extension isn't actually controlling search, so there is nothing
  // to show.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  if (!template_url_service ||
      !template_url_service->IsExtensionControlledDefaultSearch()) {
    return std::nullopt;
  }
  const TemplateURL* default_search =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search ||
      default_search->type() != TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) {
    return std::nullopt;
  }

  // For historical reasons, the search override preference is the same as the
  // one we use for the controlled home setting. We continue this so that
  // users won't see the bubble or dialog UI if they've already acknowledged
  // an older version.
  const char* preference_name =
      ControlledHomeDialogController::kAcknowledgedPreference;

  // NOTE: For most TemplateURLs, there's no guarantee that search_url is a
  // valid URL (it could contain placeholders, etc). However, for extension-
  // provided search engines, we require they be valid URLs.
  GURL search_url(default_search->url());
  DCHECK(search_url.is_valid()) << default_search->url();

  // Nothing was actually overridden; there is nothing to confirm.
  if (ExtensionSearchOverrideMatchesExistingEngine(&profile)) {
    return std::nullopt;
  }

  SecondarySearchInfo secondary_search = GetSecondarySearchInfo(profile);

  // Extension preferences live on the original profile, so an incognito
  // window sees the same stack.
  std::optional<extensions::SearchOverrideStackInfo> search_override_stack =
      extensions::GetSearchOverrideStackInfo(*profile.GetOriginalProfile(),
                                             extension->id());

  constexpr char kGenericDialogHistogramName[] =
      "Extensions.SettingsOverridden.GenericSearchOverriddenDialogResult";
  constexpr char kBackToOtherHistogramName[] =
      "Extensions.SettingsOverridden.BackToOtherSearchOverriddenDialogResult";
  constexpr char kBackToGoogleHistogramName[] =
      "Extensions.SettingsOverridden.BackToGoogleSearchOverriddenDialogResult";

  const char* histogram_name = nullptr;
  switch (secondary_search.type) {
    case SecondarySearchInfo::Type::kGoogle:
      histogram_name = kBackToGoogleHistogramName;
      break;
    case SecondarySearchInfo::Type::kNonGoogleInDefaultList:
      CHECK(!secondary_search.name.empty(), base::NotFatalUntil::M162);
      histogram_name = kBackToOtherHistogramName;
      break;
    case SecondarySearchInfo::Type::kOther:
      histogram_name = kGenericDialogHistogramName;
      break;
  }

  // The parameters fetched will depend on the style of dialog being shown.
  if (base::FeatureList::IsEnabled(
          extensions_features::kSearchEngineExplicitChoiceDialog)) {
    // Build an explicit-choice dialog.
    std::u16string dialog_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_EXPLICIT_CHOICE);
    const std::u16string extension_name_for_ui =
        extensions::ui_util::GetFixupExtensionNameForUIDisplay(
            extension->name());
    std::u16string dialog_message = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_BODY_EXPLICIT_CHOICE,
        extension_name_for_ui);

    // On the 'explicit choice' dialog, there is no icon at the title level.
    SettingsOverriddenDialogController::ShowParams show_params(
        std::move(dialog_title), std::move(dialog_message), /*icon=*/nullptr);
    ExtensionSettingsOverriddenDialog::Params params(
        extension->id(), extension->name(), preference_name, histogram_name,
        std::move(show_params));
    params.search_override_stack = search_override_stack;
    if (base::FeatureList::IsEnabled(
            features::kHappinessTrackingSurveysForDesktopSEHijacking)) {
      params.hats_survey_trigger = kHatsSurveyTriggerSEHijacking;
    }

    // Previous search engine before the extension overrode it.
    SettingsOverriddenDialogController::SettingOption& previous_setting =
        params.content.previous_setting.emplace();
    previous_setting.text = secondary_search.name;
    previous_setting.description = l10n_util::GetStringUTF16(
        IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_PREVIOUS_CHOICE);
    previous_setting.icon_source =
        MakeIconSource(secondary_search.builtin_icon_resource_id,
                       secondary_search.favicon_url);

    // New search engine from the overriding extension:
    SettingsOverriddenDialogController::SettingOption& new_setting =
        params.content.new_setting.emplace();
    new_setting.description = l10n_util::GetStringUTF16(
        IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_RECENT_CHANGE);

    // Display the known name and icon for the extension's search,
    // if it is known by the browser. Otherwise, show the formatted search URL
    // and a generic fallback icon.
    if (const auto* prepopulated_engine =
            GetPrepopulatedEngineForSearchUrl(search_url, profile)) {
      new_setting.text = prepopulated_engine->name;
      new_setting.icon_source = MakeIconSource(
          GetBuiltinSearchEngineIconResourceId(*prepopulated_engine),
          GURL(prepopulated_engine->favicon_url));
    } else {
      new_setting.text = FormatSearchUrl(search_url);
    }

    LogMissingParams(params);

    // After logging any missing parameters, attempt to fill in placeholders.
    // This is a temporary workaround until we've figured out which scenario(s)
    // yield no default search information. See http://crbug.com/513310356.
    if (previous_setting.text.empty()) {
      previous_setting.text = l10n_util::GetStringUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_DEFAULT_SEARCH_ENGINE);
    }

    return params;
  }

  const gfx::VectorIcon* icon = nullptr;
  std::u16string dialog_title;
  switch (secondary_search.type) {
    case SecondarySearchInfo::Type::kGoogle:
      dialog_title = l10n_util::GetStringUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_BACK_TO_GOOGLE);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      icon = &vector_icons::kGoogleGLogoIcon;
#endif
      break;
    case SecondarySearchInfo::Type::kNonGoogleInDefaultList:
      CHECK(!secondary_search.name.empty(), base::NotFatalUntil::M162);
      dialog_title = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_BACK_TO_OTHER,
          secondary_search.name);
      break;
    case SecondarySearchInfo::Type::kOther:
      dialog_title = l10n_util::GetStringUTF16(
          IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_GENERIC);
      break;
  }
  std::u16string dialog_message = l10n_util::GetStringFUTF16(
      IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_BODY_GENERIC,
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          search_url),
      extensions::ui_util::GetFixupExtensionNameForUIDisplay(
          extension->name()));

  SettingsOverriddenDialogController::ShowParams show_params(
      std::move(dialog_title), std::move(dialog_message), icon);
  ExtensionSettingsOverriddenDialog::Params params(
      extension->id(), extension->name(), preference_name, histogram_name,
      std::move(show_params));
  params.search_override_stack = search_override_stack;
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopSEHijacking)) {
    params.hats_survey_trigger = kHatsSurveyTriggerSEHijacking;
  }

  return params;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace settings_overridden_params
