// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search_engines_handler.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"

namespace {
// The following strings need to match with the IDs of the text input elements
// at settings/search_engines_page/search_engine_dialog.html.
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
      "setDefaultSearchEngine",
      base::BindRepeating(&SearchEnginesHandler::HandleSetDefaultSearchEngine,
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
}

void SearchEnginesHandler::OnJavascriptAllowed() {
  list_controller_.table_model()->SetObserver(this);
}

void SearchEnginesHandler::OnJavascriptDisallowed() {
  list_controller_.table_model()->SetObserver(nullptr);
  pref_change_registrar_.RemoveAll();
}

std::unique_ptr<base::DictionaryValue>
SearchEnginesHandler::GetSearchEnginesList() {
  // Find the default engine.
  const TemplateURL* default_engine =
      list_controller_.GetDefaultSearchProvider();
  int default_index =
      list_controller_.table_model()->IndexOfTemplateURL(default_engine);

  // Build the first list (default search engines).
  auto defaults = std::make_unique<base::ListValue>();
  int last_default_engine_index =
      list_controller_.table_model()->last_search_engine_index();

  // Sanity check for https://crbug.com/781703.
  CHECK_GE(last_default_engine_index, 0);

  for (int i = 0; i < last_default_engine_index; ++i) {
    // Third argument is false, as the engine is not from an extension.
    defaults->Append(CreateDictionaryForEngine(i, i == default_index));
  }

  // Build the second list (other search engines).
  auto others = std::make_unique<base::ListValue>();
  int last_other_engine_index =
      list_controller_.table_model()->last_other_engine_index();

  // Sanity check for https://crbug.com/781703.
  CHECK_LE(last_default_engine_index, last_other_engine_index);

  for (int i = std::max(last_default_engine_index, 0);
       i < last_other_engine_index; ++i) {
    others->Append(CreateDictionaryForEngine(i, i == default_index));
  }

  // Build the third list (omnibox extensions).
  auto extensions = std::make_unique<base::ListValue>();
  int engine_count = list_controller_.table_model()->RowCount();

  // Sanity check for https://crbug.com/781703.
  CHECK_LE(last_other_engine_index, engine_count);

  for (int i = std::max(last_other_engine_index, 0); i < engine_count; ++i) {
    extensions->Append(CreateDictionaryForEngine(i, i == default_index));
  }

  auto search_engines_info = std::make_unique<base::DictionaryValue>();
  search_engines_info->Set("defaults", std::move(defaults));
  search_engines_info->Set("others", std::move(others));
  search_engines_info->Set("extensions", std::move(extensions));
  return search_engines_info;
}

void SearchEnginesHandler::OnModelChanged() {
  AllowJavascript();
  FireWebUIListener("search-engines-changed", *GetSearchEnginesList());
}

void SearchEnginesHandler::OnItemsChanged(int start, int length) {
  OnModelChanged();
}

void SearchEnginesHandler::OnItemsAdded(int start, int length) {
  OnModelChanged();
}

void SearchEnginesHandler::OnItemsRemoved(int start, int length) {
  OnModelChanged();
}

std::unique_ptr<base::DictionaryValue>
SearchEnginesHandler::CreateDictionaryForEngine(int index, bool is_default) {
  TemplateURLTableModel* table_model = list_controller_.table_model();
  const TemplateURL* template_url = list_controller_.GetTemplateURL(index);

  // Sanity check for https://crbug.com/781703.
  CHECK_GE(index, 0);
  CHECK_LT(index, table_model->RowCount());
  CHECK(template_url);

  // The items which are to be written into |dict| are also described in
  // chrome/browser/resources/settings/search_engines_page/
  // in @typedef for SearchEngine. Please update it whenever you add or remove
  // any keys here.
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger("id", template_url->id());
  dict->SetString("name", template_url->short_name());
  dict->SetString("displayName",
                  table_model->GetText(
                      index, IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN));
  dict->SetString(
      "keyword",
      table_model->GetText(index, IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN));
  Profile* profile = Profile::FromWebUI(web_ui());
  dict->SetString(
      "url", template_url->url_ref().DisplayURL(UIThreadSearchTermsData()));
  dict->SetBoolean("urlLocked", template_url->prepopulate_id() > 0);
  GURL icon_url = template_url->favicon_url();
  if (icon_url.is_valid())
    dict->SetString("iconURL", icon_url.spec());
  dict->SetInteger("modelIndex", index);

  dict->SetBoolean("canBeRemoved", list_controller_.CanRemove(template_url));
  dict->SetBoolean("canBeDefault",
                   list_controller_.CanMakeDefault(template_url));
  dict->SetBoolean("default", is_default);
  dict->SetBoolean("canBeEdited", list_controller_.CanEdit(template_url));
  TemplateURL::Type type = template_url->type();
  dict->SetBoolean("isOmniboxExtension",
                   type == TemplateURL::OMNIBOX_API_EXTENSION);
  if (type == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
      type == TemplateURL::OMNIBOX_API_EXTENSION) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            template_url->GetExtensionId(),
            extensions::ExtensionRegistry::EVERYTHING);
    if (extension) {
      std::unique_ptr<base::DictionaryValue> ext_info =
          extensions::util::GetExtensionInfo(extension);
      ext_info->SetBoolean("canBeDisabled",
                           !extensions::ExtensionSystem::Get(profile)
                                ->management_policy()
                                ->MustRemainEnabled(extension, nullptr));
      dict->Set("extension", std::move(ext_info));
    }
  }
  return dict;
}

void SearchEnginesHandler::HandleGetSearchEnginesList(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  AllowJavascript();
  ResolveJavascriptCallback(*callback_id, *GetSearchEnginesList());
}

void SearchEnginesHandler::HandleSetDefaultSearchEngine(
    const base::ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }
  if (index < 0 || index >= list_controller_.table_model()->RowCount())
    return;

  list_controller_.MakeDefaultTemplateURL(index);

  base::RecordAction(base::UserMetricsAction("Options_SearchEngineSetDefault"));
}

void SearchEnginesHandler::HandleRemoveSearchEngine(
    const base::ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }
  if (index < 0 || index >= list_controller_.table_model()->RowCount())
    return;

  if (list_controller_.CanRemove(list_controller_.GetTemplateURL(index))) {
    list_controller_.RemoveTemplateURL(index);
    base::RecordAction(base::UserMetricsAction("Options_SearchEngineRemoved"));
  }
}

void SearchEnginesHandler::HandleSearchEngineEditStarted(
    const base::ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }

  // Allow -1, which means we are adding a new engine.
  if (index < kNewSearchEngineIndex ||
      index >= list_controller_.table_model()->RowCount()) {
    return;
  }

  edit_controller_ = std::make_unique<EditSearchEngineController>(
      index == kNewSearchEngineIndex ? nullptr
                                     : list_controller_.GetTemplateURL(index),
      this, Profile::FromWebUI(web_ui()));
}

void SearchEnginesHandler::OnEditedKeyword(TemplateURL* template_url,
                                           const base::string16& title,
                                           const base::string16& keyword,
                                           const std::string& url) {
  DCHECK(!url.empty());
  if (template_url)
    list_controller_.ModifyTemplateURL(template_url, title, keyword, url);
  else
    list_controller_.AddTemplateURL(title, keyword, url);

  edit_controller_.reset();
}

void SearchEnginesHandler::HandleValidateSearchEngineInput(
    const base::ListValue* args) {
  CHECK_EQ(3U, args->GetSize());

  const base::Value* callback_id;
  std::string field_name;
  std::string field_value;
  CHECK(args->Get(0, &callback_id));
  CHECK(args->GetString(1, &field_name));
  CHECK(args->GetString(2, &field_value));
  ResolveJavascriptCallback(
      *callback_id, base::Value(CheckFieldValidity(field_name, field_value)));
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
    NOTREACHED();

  return is_valid;
}

void SearchEnginesHandler::HandleSearchEngineEditCancelled(
    const base::ListValue* args) {
  if (!edit_controller_.get())
    return;
  edit_controller_->CleanUpCancelledAdd();
  edit_controller_.reset();
}

void SearchEnginesHandler::HandleSearchEngineEditCompleted(
    const base::ListValue* args) {
  if (!edit_controller_.get())
    return;
  std::string search_engine;
  std::string keyword;
  std::string query_url;
  CHECK(args->GetString(0, &search_engine));
  CHECK(args->GetString(1, &keyword));
  CHECK(args->GetString(2, &query_url));

  // Recheck validity. It's possible to get here with invalid input if e.g. the
  // user calls the right JS functions directly from the web inspector.
  if (CheckFieldValidity(kSearchEngineField, search_engine) &&
      CheckFieldValidity(kKeywordField, keyword) &&
      CheckFieldValidity(kQueryUrlField, query_url)) {
    edit_controller_->AcceptAddOrEdit(base::UTF8ToUTF16(search_engine),
                                      base::UTF8ToUTF16(keyword), query_url);
  }
}

}  // namespace settings
