// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_ENGINES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_ENGINES_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/models/table_model_observer.h"

class Profile;

namespace settings {

class SearchEnginesHandler : public SettingsPageUIHandler,
                             public ui::TableModelObserver,
                             public EditSearchEngineControllerDelegate {
 public:
  explicit SearchEnginesHandler(Profile* profile);

  SearchEnginesHandler(const SearchEnginesHandler&) = delete;
  SearchEnginesHandler& operator=(const SearchEnginesHandler&) = delete;

  ~SearchEnginesHandler() override;

  // ui::TableModelObserver implementation.
  void OnModelChanged() override;
  void OnItemsChanged(size_t start, size_t length) override;
  void OnItemsAdded(size_t start, size_t length) override;
  void OnItemsRemoved(size_t start, size_t length) override;

  // EditSearchEngineControllerDelegate implementation.
  void OnEditedKeyword(TemplateURL* template_url,
                       const std::u16string& title,
                       const std::u16string& keyword,
                       const std::string& url) override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class SearchEnginesHandlerTest;

  // Retrieves all search engines and returns them to WebUI.
  void HandleGetSearchEnginesList(const base::Value::List& args);

  base::Value::Dict GetSearchEnginesList();

  // Returns whether the search engine choice should be saved in guest mode
  // Returns null if the profile is not eligible for guest choice saving.
  // Called from WebUI.
  void HandleGetSaveGuestChoice(const base::Value::List& args);

  // Removes the search engine at the given index. Called from WebUI.
  void HandleRemoveSearchEngine(const base::Value::List& args);

  // Sets the search engine at the given index to be default. Called from WebUI.
  void HandleSetDefaultSearchEngine(const base::Value::List& args);

  // Activates or deactivates the search engine at the given index. Called from
  // WebUI.
  void HandleSetIsActiveSearchEngine(const base::Value::List& args);

  // Starts an edit session for the search engine at the given index. If the
  // index is -1, starts editing a new search engine instead of an existing one.
  // Called from WebUI.
  void HandleSearchEngineEditStarted(const base::Value::List& args);

  // Validates the given search engine values, and reports the results back
  // to WebUI. Called from WebUI.
  void HandleValidateSearchEngineInput(const base::Value::List& args);

  // Checks whether the given user input field (searchEngine, keyword, queryUrl)
  // is populated with a valid value.
  bool CheckFieldValidity(const std::string& field_name,
                          const std::string& field_value);

  // Called when an edit is canceled.
  // Called from WebUI.
  void HandleSearchEngineEditCancelled(const base::Value::List& args);

  // Called when an edit is finished and should be saved.
  // Called from WebUI.
  void HandleSearchEngineEditCompleted(const base::Value::List& args);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Request the browser to open its search settings.
  void HandleOpenBrowserSearchSettings(const base::Value::List& args);
#endif

  // Returns a dictionary to pass to WebUI representing the given search engine.
  base::Value::Dict CreateDictionaryForEngine(size_t index, bool is_default);

  const raw_ptr<Profile> profile_;

  KeywordEditorController list_controller_;
  std::unique_ptr<EditSearchEngineController> edit_controller_;
  PrefChangeRegistrar pref_change_registrar_;
  base::WeakPtrFactory<SearchEnginesHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_ENGINES_HANDLER_H_
