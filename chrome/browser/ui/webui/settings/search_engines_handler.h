// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_ENGINES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_ENGINES_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service_observer.h"

class Profile;
class TemplateURLService;

namespace search_engines {
class SearchEngineSettingsDataProvider;
}

namespace settings {

class SearchEnginesHandler : public SettingsPageUIHandler,
                             public TemplateURLServiceObserver,
                             public EditSearchEngineControllerDelegate {
 public:
  explicit SearchEnginesHandler(Profile* profile);

  SearchEnginesHandler(const SearchEnginesHandler&) = delete;
  SearchEnginesHandler& operator=(const SearchEnginesHandler&) = delete;

  ~SearchEnginesHandler() override;

  // TemplateURLServiceObserver implementation.
  void OnTemplateURLServiceChanged() override;

  // EditSearchEngineControllerDelegate implementation.
  void OnEditedKeyword(TemplateURL* template_url,
                       const std::u16string& title,
                       const std::u16string& keyword,
                       const std::string& fixed_up_url) override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class SearchEnginesHandlerTest;

  // Retrieves all search engines and returns them to WebUI.
  void HandleGetCategorizedTemplateUrls(const base::ListValue& args);

  base::DictValue GetCategorizedTemplateUrls();

  // Retrieves all search engines and returns them to WebUI.
  // TODO (crbug.com/494551138): Remove once `SearchSettingsUpdate` is launched.
  void HandleGetSearchEnginesList(const base::ListValue& args);

  base::DictValue GetSearchEnginesList();

  // Returns whether the search engine choice should be saved in guest mode
  // Returns null if the profile is not eligible for guest choice saving.
  // Called from WebUI.
  void HandleGetSaveGuestChoice(const base::ListValue& args);

  // Removes a search engine. Called from WebUI.
  // `args` contains:
  //   [0]: string: The opaque ID of the search engine to remove.
  void HandleRemoveSearchEngine(const base::ListValue& args);

  // Sets a search engine to be default. Called from WebUI.
  // `args` contains:
  //   [0]: string: The opaque ID of the search engine to make default.
  //   [1]: search_engines::ChoiceMadeLocation (int): Location where the choice
  //        was made.
  //   [2]: bool (optional): Whether to save the choice in guest mode.
  void HandleSetDefaultSearchEngine(const base::ListValue& args);

  // Activates or deactivates a search engine. Called from WebUI.
  // `args` contains:
  //   [0]: string: The opaque ID of the search engine.
  //   [1]: bool: True to activate, false to deactivate.
  void HandleSetIsActiveSearchEngine(const base::ListValue& args);

  // Starts an edit session for a search engine. If the ID is empty, starts
  // editing a new search engine instead of an existing one. Called from WebUI.
  // `args` contains:
  //   [0]: string: The opaque ID of the search engine to edit, or empty to
  //        start editing a new search engine.
  void HandleSearchEngineEditStarted(const base::ListValue& args);

  // Validates the given search engine values, and reports the results back
  // to WebUI. Called from WebUI.
  // `args` contains:
  //   [0]: string: The callback ID to resolve with the validity result.
  //   [1]: string: The field being validated ("searchEngine", "keyword", or
  //        "queryUrl").
  //   [2]: string: The field value to validate.
  void HandleValidateSearchEngineInput(const base::ListValue& args);

  // Checks whether the given user input field (searchEngine, keyword, queryUrl)
  // is populated with a valid value.
  bool CheckFieldValidity(const std::string& field_name,
                          const std::string& field_value);

  // Called when an edit is canceled.
  // Called from WebUI.
  void HandleSearchEngineEditCancelled(const base::ListValue& args);

  // Called when an edit is finished and should be saved. Called from WebUI.
  // `args` contains:
  //   [0]: string: The search engine display name.
  //   [1]: string: The keyword.
  //   [2]: string: The search query URL.
  void HandleSearchEngineEditCompleted(const base::ListValue& args);

#if BUILDFLAG(IS_CHROMEOS)
  // Request the browser to open its search settings.
  void HandleOpenBrowserSearchSettings(const base::ListValue& args);
#endif

  // Returns a dictionary to pass to WebUI representing the given search engine.
  // The engine is identified by an opaque string ID (e.g. "db:42") that the
  // WebUI sends back to reference it in subsequent messages.
  base::DictValue CreateDictionaryForEngine(TemplateURL* template_url);

  // Records the search hijacking heuristic metric if not already recorded.
  void RecordSearchHijackingHeuristicMetric();

  const raw_ptr<Profile> profile_;

  KeywordEditorController list_controller_;
  std::unique_ptr<EditSearchEngineController> edit_controller_;
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      scoped_url_service_observation_{this};

  // Prepares the engine lists shown by this handler and owns the
  // once-per-page-load settings telemetry.
  // Note: Object lifetime is how we track the page load / sessions. So do not
  // recreate it during the handler's lifetime.
  std::unique_ptr<search_engines::SearchEngineSettingsDataProvider>
      settings_data_provider_;

  bool has_recorded_hijacking_metric_ = false;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_ENGINES_HANDLER_H_
