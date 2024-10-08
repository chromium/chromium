// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search_engines_handler.h"

#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/browser/ui/webui/search_engine_choice/icon_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/new_window_delegate.h"
#endif

namespace {
// The following strings need to match with the IDs of the text input elements
// at settings/search_engines_page/search_engine_edit_dialog.html.
const char kSearchEngineField[] = "searchEngine";
const char kKeywordField[] = "keyword";
const char kQueryUrlField[] = "queryUrl";

// Dummy number used for indicating that a new search engine is added.
const int kNewSearchEngineIndex = -1;

}  // namespace

namespace settings {

SearchEnginesHandler::SearchEnginesHandler(Profile* profile)
    : profile_(profile), list_controller_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());
}

SearchEnginesHandler::~SearchEnginesHandler() {
  // TODO(tommycli): Refactor KeywordEditorController to be compatible with
  // ScopedObserver so this is no longer necessary.
  list_controller_.table_model()->SetObserver(nullptr);
}

void SearchEnginesHandler::RegisterMessages() {
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "openBrowserSearchSettings",
      base::BindRepeating(
          &SearchEnginesHandler::HandleOpenBrowserSearchSettings,
          base::Unretained(this)));
#endif
}

void SearchEnginesHandler::OnJavascriptAllowed() {
  list_controller_.table_model()->SetObserver(this);
}

void SearchEnginesHandler::OnJavascriptDisallowed() {
  list_controller_.table_model()->SetObserver(nullptr);
  pref_change_registrar_.RemoveAll();
}

base::Value::Dict SearchEnginesHandler::GetSearchEnginesList() {
  // Find the default engine.
  const TemplateURL* default_engine =
      list_controller_.GetDefaultSearchProvider();
  std::optional<size_t> default_index =
      list_controller_.table_model()->IndexOfTemplateURL(default_engine);

  // Build the first list (default search engines).
  base::Value::List defaults;
  size_t last_default_engine_index =
      list_controller_.table_model()->last_search_engine_index();

  for (size_t i = 0; i < last_default_engine_index; ++i) {
    // Third argument is false, as the engine is not from an extension.
    defaults.Append(CreateDictionaryForEngine(i, i == default_index));
  }

  // Build the second list (active search engines).
  base::Value::List actives;
  size_t last_active_engine_index =
      list_controller_.table_model()->last_active_engine_index();

  CHECK_LE(last_default_engine_index, last_active_engine_index);
  for (size_t i = last_default_engine_index; i < last_active_engine_index;
       ++i) {
    // Third argument is false, as the engine is not from an extension.
    actives.Append(CreateDictionaryForEngine(i, i == default_index));
  }

  // Build the third list (other search engines).
  base::Value::List others;
  size_t last_other_engine_index =
      list_controller_.table_model()->last_other_engine_index();

  // Sanity check for https://crbug.com/781703.
  CHECK_LE(last_active_engine_index, last_other_engine_index);

  for (size_t i = last_active_engine_index; i < last_other_engine_index; ++i) {
    others.Append(CreateDictionaryForEngine(i, i == default_index));
  }

  // Build the third list (omnibox extensions).
  base::Value::List extensions;
  size_t engine_count = list_controller_.table_model()->RowCount();

  // Sanity check for https://crbug.com/781703.
  CHECK_LE(last_other_engine_index, engine_count);

  for (size_t i = last_other_engine_index; i < engine_count; ++i) {
    extensions.Append(CreateDictionaryForEngine(i, i == default_index));
  }

  base::Value::Dict search_engines_info;
  search_engines_info.Set("defaults", std::move(defaults));
  search_engines_info.Set("actives", std::move(actives));
  search_engines_info.Set("others", std::move(others));
  search_engines_info.Set("extensions", std::move(extensions));
  return search_engines_info;
}

void SearchEnginesHandler::OnModelChanged() {
  AllowJavascript();
  FireWebUIListener("search-engines-changed", GetSearchEnginesList());
}

void SearchEnginesHandler::OnItemsChanged(size_t start, size_t length) {
  OnModelChanged();
}

void SearchEnginesHandler::OnItemsAdded(size_t start, size_t length) {
  OnModelChanged();
}

void SearchEnginesHandler::OnItemsRemoved(size_t start, size_t length) {
  OnModelChanged();
}

base::Value::Dict SearchEnginesHandler::CreateDictionaryForEngine(
    size_t index,
    bool is_default) {
  TemplateURLTableModel* table_model = list_controller_.table_model();
  const TemplateURL* template_url = list_controller_.GetTemplateURL(index);

  // Sanity check for https://crbug.com/781703.
  CHECK_LT(index, table_model->RowCount());
  CHECK(template_url);

  // The items which are to be written into |dict| are also described in
  // chrome/browser/resources/settings/search_engines_page/
  // in @typedef for SearchEngine. Please update it whenever you add or remove
  // any keys here.
  base::Value::Dict dict;
  dict.Set("id", static_cast<int>(template_url->id()));
  dict.Set("name", template_url->short_name());
  dict.Set("displayName",
           table_model->GetText(index,
                                IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN));
  dict.Set("keyword", table_model->GetKeywordToDisplay(index));
  Profile* profile = Profile::FromWebUI(web_ui());
  dict.Set("url",
           template_url->url_ref().DisplayURL(UIThreadSearchTermsData()));
  dict.Set("urlLocked", ((template_url->prepopulate_id() > 0) ||
                         (template_url->starter_pack_id() > 0)));
  GURL icon_url = template_url->favicon_url();
  if (icon_url.is_valid())
    dict.Set("iconURL", icon_url.spec());

  // The icons that are used for search engines in the EEA region are bundled
  // with Chrome. We use the favicon service for countries outside the EEA
  // region to guarantee having icons for all search engines.
  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      search_engines::SearchEngineChoiceServiceFactory::GetForProfile(profile);
  const bool is_eea_region = search_engines::IsEeaChoiceCountry(
      search_engine_choice_service->GetCountryId());
  if (is_eea_region && template_url->prepopulate_id() != 0) {
    std::string_view icon_path =
        GetSearchEngineGeneratedIconPath(template_url->keyword());
    if (!icon_path.empty()) {
      // The search engine icon path are 24px, but displayed at 16px, or 32px on
      // HiDPI screens. Use the 2x version (48px) for a large enough icon.
      // Note that this icon path is used in `site-favicon` which does not
      // support `image-set`.
      dict.Set("iconPath", base::StrCat({icon_path, "@2x"}));
    }
  }

  dict.Set("modelIndex", base::checked_cast<int>(index));

  dict.Set("canBeRemoved", list_controller_.CanRemove(template_url));
  dict.Set("canBeDefault", list_controller_.CanMakeDefault(template_url));
  dict.Set("default", is_default);
  dict.Set("canBeEdited", list_controller_.CanEdit(template_url));
  dict.Set("canBeActivated", list_controller_.CanActivate(template_url));
  dict.Set("canBeDeactivated", list_controller_.CanDeactivate(template_url));
  dict.Set("shouldConfirmDeletion",
           list_controller_.ShouldConfirmDeletion(template_url));
  dict.Set("isManaged", list_controller_.IsManaged(template_url));
  TemplateURL::Type type = template_url->type();
  dict.Set("isOmniboxExtension", type == TemplateURL::OMNIBOX_API_EXTENSION);
  dict.Set("isPrepopulated", template_url->prepopulate_id() > 0);
  dict.Set("isStarterPack", template_url->starter_pack_id() > 0);
  if (type == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
      type == TemplateURL::OMNIBOX_API_EXTENSION) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            template_url->GetExtensionId(),
            extensions::ExtensionRegistry::EVERYTHING);
    if (extension) {
      base::Value::Dict ext_info =
          extensions::util::GetExtensionInfo(extension);
      ext_info.Set("canBeDisabled",
                   !extensions::ExtensionSystem::Get(profile)
                        ->management_policy()
                        ->MustRemainEnabled(extension, nullptr));
      dict.Set("extension", std::move(ext_info));
    }
  }
  return dict;
}

void SearchEnginesHandler::HandleGetSearchEnginesList(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(callback_id, GetSearchEnginesList());
}

void SearchEnginesHandler::HandleSetDefaultSearchEngine(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  int index = args[0].GetInt();
  if (index < 0 || static_cast<size_t>(index) >=
                       list_controller_.table_model()->RowCount()) {
    return;
  }

  search_engines::ChoiceMadeLocation choice_made_location =
      static_cast<search_engines::ChoiceMadeLocation>(args[1].GetInt());
  CHECK(choice_made_location ==
            search_engines::ChoiceMadeLocation::kSearchSettings ||
        choice_made_location ==
            search_engines::ChoiceMadeLocation::kSearchEngineSettings);
  list_controller_.MakeDefaultTemplateURL(index, choice_made_location);
  base::RecordAction(base::UserMetricsAction("Options_SearchEngineSetDefault"));

  auto* choice_service =
      search_engines::SearchEngineChoiceServiceFactory::GetForProfile(profile_);
  if (!choice_service->IsProfileEligibleForDseGuestPropagation()) {
    return;
  }

  // TODO(b/364256844): Decide what to do with the subpage search engine UI.
  // CHECK(!args[2].is_none());
  bool saveGuestChoice = args[2].GetBool();
  if (!saveGuestChoice) {
    choice_service->SetSavedSearchEngineBetweenGuestSessions(std::nullopt);
    return;
  }

  int prepopulate_id =
      list_controller_.GetDefaultSearchProvider()->prepopulate_id();
  if (prepopulate_id > 0 &&
      prepopulate_id <= TemplateURLPrepopulateData::kMaxPrepopulatedEngineID) {
    choice_service->SetSavedSearchEngineBetweenGuestSessions(prepopulate_id);
  }
}

void SearchEnginesHandler::HandleGetSaveGuestChoice(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();

  base::Value save_guest_choice;
  auto* choice_service =
      search_engines::SearchEngineChoiceServiceFactory::GetForProfile(profile_);
  if (choice_service->IsProfileEligibleForDseGuestPropagation()) {
    save_guest_choice = base::Value(
        choice_service->GetSavedSearchEngineBetweenGuestSessions().has_value());
  }
  ResolveJavascriptCallback(callback_id, std::move(save_guest_choice));
}

void SearchEnginesHandler::HandleSetIsActiveSearchEngine(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const int index = args[0].GetInt();
  const bool is_active = args[1].GetBool();

  if (index < 0 || static_cast<size_t>(index) >=
                       list_controller_.table_model()->RowCount()) {
    return;
  }

  list_controller_.SetIsActiveTemplateURL(index, is_active);
}

void SearchEnginesHandler::HandleRemoveSearchEngine(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  int index = args[0].GetInt();
  if (index < 0 || static_cast<size_t>(index) >=
                       list_controller_.table_model()->RowCount()) {
    return;
  }

  if (list_controller_.CanRemove(list_controller_.GetTemplateURL(index))) {
    list_controller_.RemoveTemplateURL(index);
    base::RecordAction(base::UserMetricsAction("Options_SearchEngineRemoved"));
  }
}

void SearchEnginesHandler::HandleSearchEngineEditStarted(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  int index = args[0].GetInt();

  TemplateURL* engine = nullptr;
  if (index >= 0 &&
      static_cast<size_t>(index) < list_controller_.table_model()->RowCount()) {
    engine = list_controller_.GetTemplateURL(index);
  } else if (index != kNewSearchEngineIndex) {
    return;
  }

  edit_controller_ = std::make_unique<EditSearchEngineController>(
      engine, this, Profile::FromWebUI(web_ui()));
}

void SearchEnginesHandler::OnEditedKeyword(TemplateURL* template_url,
                                           const std::u16string& title,
                                           const std::u16string& keyword,
                                           const std::string& url) {
  DCHECK(!url.empty());
  if (template_url)
    list_controller_.ModifyTemplateURL(template_url, title, keyword, url);
  else
    list_controller_.AddTemplateURL(title, keyword, url);

  edit_controller_.reset();
}

void SearchEnginesHandler::HandleValidateSearchEngineInput(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());

  const base::Value& callback_id = args[0];
  const std::string& field_name = args[1].GetString();
  const std::string& field_value = args[2].GetString();
  ResolveJavascriptCallback(
      callback_id, base::Value(CheckFieldValidity(field_name, field_value)));
}

bool SearchEnginesHandler::CheckFieldValidity(const std::string& field_name,
                                              const std::string& field_value) {
  if (!edit_controller_.get())
    return false;

  bool is_valid = false;
  if (field_name.compare(kSearchEngineField) == 0)
    is_valid = edit_controller_->IsTitleValid(base::UTF8ToUTF16(field_value));
  else if (field_name.compare(kKeywordField) == 0)
    is_valid = edit_controller_->IsKeywordValid(base::UTF8ToUTF16(field_value));
  else if (field_name.compare(kQueryUrlField) == 0)
    is_valid = edit_controller_->IsURLValid(field_value);
  else
    NOTREACHED_IN_MIGRATION();

  return is_valid;
}

void SearchEnginesHandler::HandleSearchEngineEditCancelled(
    const base::Value::List& args) {
  if (!edit_controller_.get())
    return;
  edit_controller_->CleanUpCancelledAdd();
  edit_controller_.reset();
}

void SearchEnginesHandler::HandleSearchEngineEditCompleted(
    const base::Value::List& args) {
  if (!edit_controller_.get())
    return;

  CHECK_EQ(3U, args.size());
  const std::string& search_engine = args[0].GetString();
  const std::string& keyword = args[1].GetString();
  const std::string& query_url = args[2].GetString();

  // Recheck validity. It's possible to get here with invalid input if e.g. the
  // user calls the right JS functions directly from the web inspector.
  if (CheckFieldValidity(kSearchEngineField, search_engine) &&
      CheckFieldValidity(kKeywordField, keyword) &&
      CheckFieldValidity(kQueryUrlField, query_url)) {
    edit_controller_->AcceptAddOrEdit(base::UTF8ToUTF16(search_engine),
                                      base::UTF8ToUTF16(keyword), query_url);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SearchEnginesHandler::HandleOpenBrowserSearchSettings(
    const base::Value::List& args) {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kSearchSubPage),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kSwitchToTab);
}
#endif

}  // namespace settings
