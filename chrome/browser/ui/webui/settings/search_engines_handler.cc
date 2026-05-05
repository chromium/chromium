// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search_engines_handler.h"

#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/search_hijacking_detector.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_id.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/ui_utils.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/new_window_delegate.h"
#endif

namespace {
// The following strings need to match with the IDs of the text input elements
// at settings/search_engines_page/search_engine_edit_dialog.html.
const char kSearchEngineField[] = "searchEngine";
const char kKeywordField[] = "keyword";
const char kQueryUrlField[] = "queryUrl";

void ProcessGuestDsePropagation(Profile& profile,
                                bool save_guest_choice,
                                int dse_prepopulate_id) {
  auto* choice_service =
      search_engines::SearchEngineChoiceServiceFactory::GetForProfile(&profile);
  if (!choice_service->IsDsePropagationAllowedForGuest()) {
    return;
  }

  if (!save_guest_choice) {
    // The user opted out, so clear the propagated choice. The next Guest
    // session will be reprompted if a choice is needed.
    choice_service->SetSavedSearchEngineBetweenGuestSessions(std::nullopt);
    return;
  }

  if (dse_prepopulate_id <= 0 ||
      dse_prepopulate_id >
          TemplateURLPrepopulateData::kMaxPrepopulatedEngineID) {
    // DSE is custom or coming from local overrides, and incompatible with
    // propagation.
    return;
  }

  choice_service->SetSavedSearchEngineBetweenGuestSessions(dse_prepopulate_id);
}

std::u16string GetDisplayName(std::u16string url_short_name, bool is_default) {
  // TODO(crbug.com/41290309): Consider adding a special case if the short name
  // is a URL, since those should always be displayed LTR.
  base::i18n::AdjustStringForLocaleDirection(&url_short_name);
  return is_default
             ? l10n_util::GetStringFUTF16(
                   IDS_SEARCH_ENGINES_EDITOR_DEFAULT_ENGINE, url_short_name)
             : url_short_name;
}

}  // namespace

namespace settings {

SearchEnginesHandler::SearchEnginesHandler(Profile* profile)
    : profile_(profile), list_controller_(profile) {}

SearchEnginesHandler::~SearchEnginesHandler() = default;

void SearchEnginesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getCategorizedTemplateUrls",
      base::BindRepeating(
          &SearchEnginesHandler::HandleGetCategorizedTemplateUrls,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSearchEnginesList",
      base::BindRepeating(&SearchEnginesHandler::HandleGetSearchEnginesList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSaveGuestChoice",
      base::BindRepeating(&SearchEnginesHandler::HandleGetSaveGuestChoice,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDefaultSearchEngine",
      base::BindRepeating(&SearchEnginesHandler::HandleSetDefaultSearchEngine,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setIsActiveSearchEngine",
      base::BindRepeating(&SearchEnginesHandler::HandleSetIsActiveSearchEngine,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeSearchEngine",
      base::BindRepeating(&SearchEnginesHandler::HandleRemoveSearchEngine,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "validateSearchEngineInput",
      base::BindRepeating(
          &SearchEnginesHandler::HandleValidateSearchEngineInput,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "searchEngineEditStarted",
      base::BindRepeating(&SearchEnginesHandler::HandleSearchEngineEditStarted,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "searchEngineEditCancelled",
      base::BindRepeating(
          &SearchEnginesHandler::HandleSearchEngineEditCancelled,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "searchEngineEditCompleted",
      base::BindRepeating(
          &SearchEnginesHandler::HandleSearchEngineEditCompleted,
          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "openBrowserSearchSettings",
      base::BindRepeating(
          &SearchEnginesHandler::HandleOpenBrowserSearchSettings,
          base::Unretained(this)));
#endif
}

void SearchEnginesHandler::OnJavascriptAllowed() {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  CHECK(template_url_service);
  scoped_url_service_observation_.Observe(template_url_service);
  list_controller_.Refresh();
}

void SearchEnginesHandler::OnJavascriptDisallowed() {
  scoped_url_service_observation_.Reset();
}

base::DictValue SearchEnginesHandler::GetCategorizedTemplateUrls() {
  base::DictValue search_engines_data;
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(profile);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  CHECK(template_url_service);

  bool ai_mode_enabled = OmniboxFieldTrial::IsAimStarterPackEnabled(
      AimEligibilityServiceFactory::GetForProfile(profile));
  bool gemini_enabled =
      base::FeatureList::IsEnabled(omnibox::kStarterPackExpansion) &&
      profile->GetPrefs()->GetInteger(prefs::kGeminiSettings) == 0;

  TemplateURLService::CategorizedTemplateUrls data =
      template_url_service->GetCategorizedTemplateURLs(
          internal::GetDisabledStarterPackIds(ai_mode_enabled, gemini_enabled));

  auto transform_urls =
      [&](const TemplateURL::TemplateURLVector& template_urls) {
        base::ListValue transformed_list;
        for (const auto& template_url : template_urls) {
          transformed_list.Append(CreateDictionaryForEngine(template_url));
        }
        return transformed_list;
      };

  search_engines_data.Set("activeSiteShortcuts",
                          transform_urls(data.active_site_shortcuts));
  search_engines_data.Set("inactiveSiteShortcuts",
                          transform_urls(data.inactive_site_shortcuts));
  search_engines_data.Set("activeFeatureShortcuts",
                          transform_urls(data.active_feature_shortcuts));
  search_engines_data.Set("inactiveFeatureShortcuts",
                          transform_urls(data.inactive_feature_shortcuts));
  return search_engines_data;
}

base::DictValue SearchEnginesHandler::GetSearchEnginesList() {
  CHECK(!base::FeatureList::IsEnabled(switches::kSearchSettingsUpdate));

  // Build the first list (default search engines).
  base::ListValue defaults;
  size_t last_default_engine_index =
      list_controller_.table_model()->last_search_engine_index();

  for (size_t i = 0; i < last_default_engine_index; ++i) {
    // Third argument is false, as the engine is not from an extension.
    defaults.Append(
        CreateDictionaryForEngine(list_controller_.GetTemplateURLForIndex(i)));
  }

  // Build the second list (active search engines).
  base::ListValue actives;
  size_t last_active_engine_index =
      list_controller_.table_model()->last_active_engine_index();

  CHECK_LE(last_default_engine_index, last_active_engine_index);
  for (size_t i = last_default_engine_index; i < last_active_engine_index;
       ++i) {
    actives.Append(
        CreateDictionaryForEngine(list_controller_.GetTemplateURLForIndex(i)));
  }

  // Build the third list (other search engines).
  base::ListValue others;
  size_t last_other_engine_index =
      list_controller_.table_model()->last_other_engine_index();

  // Sanity check for https://crbug.com/40548229.
  CHECK_LE(last_active_engine_index, last_other_engine_index);

  for (size_t i = last_active_engine_index; i < last_other_engine_index; ++i) {
    others.Append(
        CreateDictionaryForEngine(list_controller_.GetTemplateURLForIndex(i)));
  }

  // Build the third list (omnibox extensions).
  base::ListValue extensions;
  size_t engine_count = list_controller_.table_model()->engine_count();

  // Sanity check for https://crbug.com/40548229.
  CHECK_LE(last_other_engine_index, engine_count);

  for (size_t i = last_other_engine_index; i < engine_count; ++i) {
    extensions.Append(
        CreateDictionaryForEngine(list_controller_.GetTemplateURLForIndex(i)));
  }

  base::DictValue search_engines_info;
  search_engines_info.Set("defaults", std::move(defaults));
  search_engines_info.Set("actives", std::move(actives));
  search_engines_info.Set("others", std::move(others));
  search_engines_info.Set("extensions", std::move(extensions));
  return search_engines_info;
}

void SearchEnginesHandler::OnTemplateURLServiceChanged() {
  AllowJavascript();

  list_controller_.Refresh();

  FireWebUIListener(
      "search-engines-changed",
      base::FeatureList::IsEnabled(switches::kSearchSettingsUpdate)
          ? GetCategorizedTemplateUrls()
          : GetSearchEnginesList());
}

base::DictValue SearchEnginesHandler::CreateDictionaryForEngine(
    TemplateURL* template_url) {
  CHECK(template_url);

  bool is_default = template_url == list_controller_.GetDefaultSearchProvider();

  // The items which are to be written into |dict| are also described in
  // chrome/browser/resources/settings/search_engines_page/
  // in @typedef for SearchEngine. Please update it whenever you add or remove
  // any keys here.
  base::DictValue dict;
  dict.Set("id", static_cast<int>(template_url->id()));
  dict.Set("name", template_url->short_name());
  dict.Set("displayName",
           GetDisplayName(template_url->short_name(), is_default));
  dict.Set("keyword", base::i18n::GetDisplayStringInLTRDirectionality(
                          template_url->keyword()));
  Profile* profile = Profile::FromWebUI(web_ui());
  dict.Set("url",
           template_url->url_ref().DisplayURL(UIThreadSearchTermsData()));
  dict.Set("urlLocked",
           ((template_url->prepopulate_id() > 0) ||
            (template_url->starter_pack_id() !=
             template_url_starter_pack_data::StarterPackId::kNone)));
  GURL icon_url = template_url->favicon_url();
  if (icon_url.is_valid()) {
    dict.Set("iconURL", icon_url.spec());
  } else if (template_url->CreatedByEnterpriseSearchAggregatorPolicy()) {
    // The icon used for search aggregator is bundled with Chrome and should
    // be used as a fallback if the icon_url is not set.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    dict.Set("iconPath",
             base::FeatureList::IsEnabled(omnibox::kUseAgentspace25Logo)
                 ? "chrome://theme/IDR_GOOGLE_AGENTSPACE_LOGO_25"
                 : "chrome://theme/IDR_GOOGLE_AGENTSPACE_LOGO");
#endif
  }

  // The icons that are used for search engines in the EEA region are bundled
  // with Chrome. We use the favicon service for countries outside the EEA
  // region to guarantee having icons for all search engines.
  // There may not be a resource ID associated with this template URL, e.g. for
  // starter pack engines or non-branded builds. In this case, do not set the
  // icon path but let the WebUI handle the fallback logic instead.
  regional_capabilities::RegionalCapabilitiesService* regional_capabilities =
      regional_capabilities::RegionalCapabilitiesServiceFactory::GetForProfile(
          profile);
  const bool is_eea_region = regional_capabilities->IsInEeaCountry();
  if (is_eea_region && template_url->GetBaseBuiltinResourceId().has_value()) {
    // The search engine icon path are 24px, but displayed at 16px, or 32px on
    // HiDPI screens. Use the 2x version (48px) for a large enough icon.
    // Note that this icon path is used in `site-favicon` which does not
    // support `image-set`.
    dict.Set("iconPath",
             base::StrCat({"chrome://theme/",
                           template_url->GetBuiltinImageResourceId(), "@2x"}));
  }

  dict.Set("canBeRemoved", list_controller_.CanRemove(template_url));
  dict.Set("canBeDefault", list_controller_.CanMakeDefault(template_url));
  dict.Set("default", is_default);
  dict.Set("canBeEdited", list_controller_.CanEdit(template_url));
  dict.Set("canBeActivated", list_controller_.CanActivate(template_url));
  dict.Set("canBeDeactivated", list_controller_.CanDeactivate(template_url));
  dict.Set("shouldConfirmRemoval",
           list_controller_.ShouldConfirmRemoval(template_url));
  dict.Set("isManaged", list_controller_.IsManaged(template_url));
  TemplateURL::Type type = template_url->type();
  dict.Set("isOmniboxExtension", type == TemplateURL::OMNIBOX_API_EXTENSION);
  dict.Set("isPrepopulated", template_url->prepopulate_id() > 0);
  dict.Set("isStarterPack",
           template_url->starter_pack_id() !=
               template_url_starter_pack_data::StarterPackId::kNone);
  if (type == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
      type == TemplateURL::OMNIBOX_API_EXTENSION) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            template_url->GetExtensionId(),
            extensions::ExtensionRegistry::EVERYTHING);
    if (extension) {
      base::DictValue ext_info = extensions::util::GetExtensionInfo(extension);
      ext_info.Set("canBeDisabled",
                   !extensions::ExtensionSystem::Get(profile)
                        ->management_policy()
                        ->MustRemainEnabled(extension, nullptr));
      dict.Set("extension", std::move(ext_info));
    }
  }
  return dict;
}

void SearchEnginesHandler::RecordSearchHijackingHeuristicMetric() {
  if (has_recorded_hijacking_metric_) {
    return;
  }

  auto status = safe_browsing::SearchHijackingDetector::GetPriorHeuristicResult(
      profile_->GetPrefs());

  bool available =
      (status !=
       safe_browsing::SearchHijackingDetector::HeuristicResult::kUnknown);

  base::UmaHistogramBoolean(
      "Settings.SearchEngines.SearchHijackingDetector.HeuristicAvailable",
      available);

  if (available) {
    base::UmaHistogramBoolean(
        "Settings.SearchEngines.SearchHijackingDetector.HeuristicMatch",
        status ==
            safe_browsing::SearchHijackingDetector::HeuristicResult::kMatch);
  }

  has_recorded_hijacking_metric_ = true;
}

void SearchEnginesHandler::HandleGetCategorizedTemplateUrls(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();

  RecordSearchHijackingHeuristicMetric();

  ResolveJavascriptCallback(callback_id, GetCategorizedTemplateUrls());
}

void SearchEnginesHandler::HandleGetSearchEnginesList(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();

  RecordSearchHijackingHeuristicMetric();

  ResolveJavascriptCallback(callback_id, GetSearchEnginesList());
}

void SearchEnginesHandler::HandleSetDefaultSearchEngine(
    const base::ListValue& args) {
  CHECK_EQ(3U, args.size());
  TemplateURLID id = args[0].GetInt();

  search_engines::ChoiceMadeLocation choice_made_location =
      static_cast<search_engines::ChoiceMadeLocation>(args[1].GetInt());
  CHECK(choice_made_location ==
            search_engines::ChoiceMadeLocation::kSearchSettings ||
        choice_made_location ==
            search_engines::ChoiceMadeLocation::kSearchEngineSettings);
  list_controller_.MakeDefaultTemplateURL(id, choice_made_location);
  base::RecordAction(base::UserMetricsAction("Options_SearchEngineSetDefault"));

  if (std::optional<bool> save_guest_choice = args[2].GetIfBool();
      save_guest_choice.has_value()) {
    ProcessGuestDsePropagation(
        *profile_, save_guest_choice.value(),
        list_controller_.GetDefaultSearchProvider()->prepopulate_id());
  }
}

void SearchEnginesHandler::HandleGetSaveGuestChoice(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();

  base::Value save_guest_choice;
  auto* choice_service =
      search_engines::SearchEngineChoiceServiceFactory::GetForProfile(profile_);
  if (choice_service->IsDsePropagationAllowedForGuest()) {
    save_guest_choice = base::Value(
        choice_service->GetSavedSearchEngineBetweenGuestSessions().has_value());
  }
  ResolveJavascriptCallback(callback_id, std::move(save_guest_choice));
}

void SearchEnginesHandler::HandleSetIsActiveSearchEngine(
    const base::ListValue& args) {
  CHECK_EQ(2U, args.size());
  const TemplateURLID id = args[0].GetInt();
  const bool is_active = args[1].GetBool();

  list_controller_.SetIsActiveTemplateURL(id, is_active);
}

void SearchEnginesHandler::HandleRemoveSearchEngine(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  TemplateURLID id = args[0].GetInt();

  TemplateURL* template_url = list_controller_.GetTemplateURL(id);
  if (template_url && list_controller_.CanRemove(template_url)) {
    list_controller_.RemoveTemplateURL(id);
    base::RecordAction(base::UserMetricsAction("Options_SearchEngineRemoved"));
  }
}

void SearchEnginesHandler::HandleSearchEngineEditStarted(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  TemplateURLID id = args[0].GetInt();

  TemplateURL* engine = list_controller_.GetTemplateURL(id);

  if (!engine && id != kInvalidTemplateURLID) {
    return;
  }

  edit_controller_ = std::make_unique<EditSearchEngineController>(
      engine, this, Profile::FromWebUI(web_ui()));
}

void SearchEnginesHandler::OnEditedKeyword(TemplateURL* template_url,
                                           const std::u16string& title,
                                           const std::u16string& keyword,
                                           const std::string& fixed_up_url) {
  CHECK(!fixed_up_url.empty());
  if (template_url) {
    list_controller_.ModifyTemplateURL(template_url, title, keyword,
                                       fixed_up_url);
  } else {
    list_controller_.AddTemplateURL(title, keyword, fixed_up_url);
  }

  edit_controller_.reset();
}

void SearchEnginesHandler::HandleValidateSearchEngineInput(
    const base::ListValue& args) {
  CHECK_EQ(3U, args.size());

  const base::Value& callback_id = args[0];
  const std::string& field_name = args[1].GetString();
  const std::string& field_value = args[2].GetString();
  ResolveJavascriptCallback(
      callback_id, base::Value(CheckFieldValidity(field_name, field_value)));
}

bool SearchEnginesHandler::CheckFieldValidity(const std::string& field_name,
                                              const std::string& field_value) {
  if (!edit_controller_.get()) {
    return false;
  }

  bool is_valid = false;
  if (field_name.compare(kSearchEngineField) == 0) {
    is_valid = edit_controller_->IsTitleValid(base::UTF8ToUTF16(field_value));
  } else if (field_name.compare(kKeywordField) == 0) {
    is_valid = edit_controller_->IsKeywordValid(base::UTF8ToUTF16(field_value));
  } else if (field_name.compare(kQueryUrlField) == 0) {
    is_valid = edit_controller_->IsURLValid(field_value);
  } else {
    NOTREACHED();
  }

  return is_valid;
}

void SearchEnginesHandler::HandleSearchEngineEditCancelled(
    const base::ListValue& args) {
  if (!edit_controller_.get()) {
    return;
  }
  edit_controller_->CleanUpCancelledAdd();
  edit_controller_.reset();
}

void SearchEnginesHandler::HandleSearchEngineEditCompleted(
    const base::ListValue& args) {
  if (!edit_controller_.get()) {
    return;
  }

  CHECK_EQ(3U, args.size());
  const std::string& search_engine = args[0].GetString();
  const std::string& keyword = args[1].GetString();
  const std::string& query_url = args[2].GetString();

  // Recheck validity. It's possible to get here with invalid input if e.g.
  // the user calls the right JS functions directly from the web inspector.
  if (CheckFieldValidity(kSearchEngineField, search_engine) &&
      CheckFieldValidity(kKeywordField, keyword) &&
      CheckFieldValidity(kQueryUrlField, query_url)) {
    edit_controller_->AcceptAddOrEdit(base::UTF8ToUTF16(search_engine),
                                      base::UTF8ToUTF16(keyword), query_url);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void SearchEnginesHandler::HandleOpenBrowserSearchSettings(
    const base::ListValue& args) {
  ash::NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kSearchSubPage),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kSwitchToTab);
}
#endif

}  // namespace settings
