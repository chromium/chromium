// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_ENGINES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_ENGINES_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/models/table_model_observer.h"

class Profile;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class Extension;
}

namespace settings {

class SearchEnginesHandler : public SettingsPageUIHandler,
                             public ui::TableModelObserver,
                             public EditSearchEngineControllerDelegate {
 public:
  explicit SearchEnginesHandler(Profile* profile);
  ~SearchEnginesHandler() override;

  // ui::TableModelObserver implementation.
  void OnModelChanged() override;
  void OnItemsChanged(int start, int length) override;
  void OnItemsAdded(int start, int length) override;
  void OnItemsRemoved(int start, int length) override;

  // EditSearchEngineControllerDelegate implementation.
  void OnEditedKeyword(TemplateURL* template_url,
                       const base::string16& title,
                       const base::string16& keyword,
                       const std::string& url) override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Retrieves all search engines and returns them to WebUI.
  void HandleGetSearchEnginesList(const base::ListValue* args);

  std::unique_ptr<base::DictionaryValue> GetSearchEnginesList();

  // Removes the search engine at the given index. Called from WebUI.
  void HandleRemoveSearchEngine(const base::ListValue* args);

  // Sets the search engine at the given index to be default. Called from WebUI.
  void HandleSetDefaultSearchEngine(const base::ListValue* args);

  // Starts an edit session for the search engine at the given index. If the
  // index is -1, starts editing a new search engine instead of an existing one.
  // Called from WebUI.
  void HandleSearchEngineEditStarted(const base::ListValue* args);

  // Validates the given search engine values, and reports the results back
  // to WebUI. Called from WebUI.
  void HandleValidateSearchEngineInput(const base::ListValue* args);

  // Checks whether the given user input field (searchEngine, keyword, queryUrl)
  // is populated with a valid value.
  bool CheckFieldValidity(const std::string& field_name,
                          const std::string& field_value);

  // Called when an edit is canceled.
  // Called from WebUI.
  void HandleSearchEngineEditCancelled(const base::ListValue* args);

  // Called when an edit is finished and should be saved.
  // Called from WebUI.
  void HandleSearchEngineEditCompleted(const base::ListValue* args);

  // Returns a dictionary to pass to WebUI representing the given search engine.
  std::unique_ptr<base::DictionaryValue> CreateDictionaryForEngine(
      int index,
      bool is_default);

  // Returns a dictionary to pass to WebUI representing the extension.
  base::DictionaryValue* CreateDictionaryForExtension(
      const extensions::Extension& extension);

  Profile* const profile_;

  KeywordEditorController list_controller_;
  std::unique_ptr<EditSearchEngineController> edit_controller_;
  PrefChangeRegistrar pref_change_registrar_;
  base::WeakPtrFactory<SearchEnginesHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_ENGINES_HANDLER_H_
