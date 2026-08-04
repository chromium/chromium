// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_SEARCHBOX_HANDLER_H_

#include <optional>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_observer.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/pref_names.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/browser/searchbox_utils.h"
#include "components/omnibox/common/input_state.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/omnibox_proto/chrome_searchbox_stats.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class GURL;
class OmniboxController;
class OmniboxClient;
class Profile;
class OmniboxEditModel;

namespace content {
class WebContents;
}  // namespace content

namespace searchbox_internal {
// Internal constants for icon resource paths shared by SearchboxHandler and its
// subclasses.
extern const char* kSearchSparkIconResourceName;
extern const char* kReplyRotated180IconResourceName;
}  // namespace searchbox_internal

namespace gfx {
class Size;
}  // namespace gfx

// Base class for browser-side handlers that handle bi-directional communication
// with WebUI search boxes.

// This just allows declaration in class to avoid cluttering global namespace.
#define DECLARE_FEATURE(feature) static constinit const base::Feature feature

class SearchboxHandler : public searchbox::mojom::PageHandler,
                         public AutocompleteController::Observer,
                         public PermissionPromptObserver::Observer {
 public:
  class Delegate {
   public:
    virtual void OnEmbeddedPermissionDialogChanged(
        bool is_showing,
        const gfx::Size& prompt_size) = 0;
    virtual OmniboxController* GetOmniboxController();
  };

  SearchboxHandler(const SearchboxHandler&) = delete;
  SearchboxHandler& operator=(const SearchboxHandler&) = delete;

  struct WebUIDataSourceOptions {
    bool enable_voice_search = false;
    bool enable_lens_search = false;
    bool session_allows_drag_and_drop = false;
    bool is_lens = false;
  };

  static bool GetAllVoiceSearchCoherenceComposeboxesEnabled();
  static bool GetVoiceSearchCoherenceAnySearchboxExperimentEnabled();
  static bool GetVoiceSearchCoherenceCobrowsingComposeboxEnabled();

  static base::DictValue GetWebUIDataSourceDict(Profile* profile);
  static base::DictValue GetWebUIDataSourceDict(Profile* profile,
                                                WebUIDataSourceOptions options);

  // Maps all icons returned from either `AutocompleteMatch::GetVectorIcon()` or
  // `OmniboxAction::GetIconImage()` to svg resource strings.
  virtual std::string AutocompleteIconToResourceName(
      const gfx::VectorIcon& icon) const;

  // Adds file context to the searchbox from the browser.
  void AddFileContextFromBrowser(
      base::UnguessableToken token,
      searchbox::mojom::SelectedFileInfoPtr file_info);

  // Notifies the WebUI that the contextual input status has changed.
  void OnContextualInputStatusChanged(
      base::UnguessableToken token,
      contextual_search::ContextUploadStatus status,
      std::optional<contextual_search::ContextUploadErrorType> error_type);

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // PermissionPromptObserver::Observer:
  void OnPermissionPromptChanged(bool is_showing,
                                 const gfx::Size& prompt_size) override;

  // searchbox::mojom::PageHandler:
  void OnFocusChanged(bool focused) override;
  void QueryAutocomplete(int32_t query_id,
                         const std::u16string& input,
                         bool prevent_inline_autocomplete,
                         uint32_t cursor_position,
                         omnibox::SuggestInventory suggest_inventory,
                         bool is_on_focus) override;
  void StopAutocomplete(bool clear_result) override;
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             uint8_t mouse_button,
                             searchbox::mojom::ActionModifiersPtr modifiers,
                             bool via_keyboard) override;
  void SetSmartComposeStats(
      searchbox::mojom::SmartComposeStatsPtr smart_compose_stats) override {}
  void SetInputMethod(searchbox::mojom::InputMethod input_method) override;
  void SetPopupSelection(
      searchbox::mojom::OmniboxPopupSelectionPtr selection) override;
  void OpenPopupSelection(uint32_t result_sequence_id,
                          searchbox::mojom::OmniboxPopupSelectionPtr selection,
                          WindowOpenDisposition disposition) override;
  void OnNavigationLikely(
      uint8_t line,
      const GURL& url,
      omnibox::mojom::NavigationPredictor navigation_predictor) override;
  void DeleteAutocompleteMatch(uint8_t line, const GURL& url) override;
  void ActivateKeyword(uint8_t line,
                       const GURL& url,
                       base::TimeTicks match_selection_timestamp,
                       bool is_mouse_event) override;
  void ExecuteAction(uint8_t line,
                     uint8_t action_index,
                     const GURL& url,
                     base::TimeTicks match_selection_timestamp,
                     uint8_t mouse_button,
                     bool alt_key,
                     bool ctrl_key,
                     bool meta_key,
                     bool shift_key) override;
  void GetCyclingPlaceholderConfig(
      GetCyclingPlaceholderConfigCallback callback) override;
  void GetRecentTabs(GetRecentTabsCallback callback) override;
  void GetTabPreview(int32_t tab_id, GetTabPreviewCallback callback) override {}
  void WaitForTabFaviconLoad(int32_t tab_id,
                             WaitForTabFaviconLoadCallback callback) override;
  void GetInputState(GetInputStateCallback callback) override;
  void NotifySessionStarted() override {}
  void NotifySessionAbandoned() override {}
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override {}
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     AddTabContextCallback) override {}
  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override {}
  void DeleteTabContext(int32_t tab_id) override {}
  void ClearFiles(bool should_block_auto_suggested_tabs) override {}
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key,
                   bool is_voice_search) override {}
  void OpenLensSearch() override {}
  void SetActiveToolMode(omnibox::ToolMode tool) override {}
  void RecordToolSelectionAction(omnibox::ToolMode tool) override {}
  void SetActiveModelMode(omnibox::ModelMode model) override {}
  void RecordModelSelectionAction(omnibox::ModelMode model) override {}
  void ActivateMetricsFunnel(const std::string& funnel_name) override {}
  void GetDriveDisclaimerStatus(
      GetDriveDisclaimerStatusCallback callback) override;
  void OnDriveDisclaimerAccepted() override;
  void OnDriveUploadClicked(OnDriveUploadClickedCallback callback) override;
  void OpenProfilePicker() override {}
  void GetPageClassification(GetPageClassificationCallback callback) override;
#if !BUILDFLAG(IS_ANDROID)
  void SetSmartTabSharingActive(bool active) override;
  void GetSmartTabSharingActive(
      GetSmartTabSharingActiveCallback callback) override;
#endif
  void set_delegate(Delegate* delegate) { omnibox_delegate_ = delegate; }

 protected:
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest, AutocompleteController_Start);
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest,
                           AutocompleteController_StartWithSuggestInventory);
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest, SetInputMethodTest);
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest, RealboxUpdatesEditModelInput);
  FRIEND_TEST_ALL_PREFIXES(LensSearchboxHandlerTest,
                           Lens_AutocompleteController_Start);
  FRIEND_TEST_ALL_PREFIXES(WebuiOmniboxHandlerTest,
                           OpenAutocompleteMatch_KeyboardModifiers);
  FRIEND_TEST_ALL_PREFIXES(WebuiOmniboxHandlerTest, OpenLensSearch);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTest,
                           QueryAutocomplete_SetsLensInputs);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTest,
                           QueryAutocomplete_SetsLensInputs_InToolModes);
  SearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_page,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<OmniboxClient> client,
      std::optional<base::TimeDelta> autocomplete_stop_timer_duration =
          std::nullopt);

  ~SearchboxHandler() override;

  OmniboxController* omnibox_controller() const;
  OmniboxClient* client() const;
  AutocompleteController* autocomplete_controller() const;
  OmniboxEditModel* edit_model() const;
  searchbox::mojom::Page* page() { return page_.get(); }

  const AutocompleteMatch* GetMatchWithUrl(size_t index, const GURL& url) const;

  virtual omnibox::InputState GetInputState() const;
  virtual std::string GetPreviousQuery();

  void SetAutocompleteControllerForTesting(
      std::unique_ptr<AutocompleteController> controller);

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  // Tracks the ID of the latest query received from the page and sent to
  // autocompletion. The ID is sent from the page along with the query details,
  // and returned to the page along with the autocomplete result. The page uses
  // it to filter out stale async results when it has began sending a new query
  // that hasn't yet traversed the mojom layer yet.
  int32_t current_query_id_ = -1;
  raw_ptr<OmniboxController> controller_;
  raw_ptr<Delegate> omnibox_delegate_;

  std::unique_ptr<OmniboxController> owned_controller_;
  std::unique_ptr<OmniboxClient> client_;
  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  searchbox::InteractionMetricsTracker metrics_tracker_;

  base::ScopedObservation<AutocompleteController,
                          AutocompleteController::Observer>
      autocomplete_controller_observation_{this};

  // TODO(crbug.com/534328951): This is not ideal state to keep in the
  //   SearchboxHandler since it related to the `AutocompleteInput`. Instead
  //   create the AutocompleteInput first.
  // This is needed in order to keep track of the last input method without
  // needing to call an async getter from the `page_`.
  omnibox::metrics::ChromeSearchboxStats::InputMethod input_method_ =
      omnibox::metrics::ChromeSearchboxStats::KEYBOARD;

  mojo::Receiver<searchbox::mojom::PageHandler> page_handler_;
  mojo::Remote<searchbox::mojom::Page> page_;
  base::WeakPtrFactory<SearchboxHandler> weak_ptr_factory_{this};

  void OpenMatch(OmniboxPopupSelection selection,
                 AutocompleteMatch match,
                 WindowOpenDisposition disposition,
                 base::TimeTicks match_selection_timestamp);

  void OnDefaultSearchExtensionDialogDone(
      OmniboxPopupSelection selection,
      AutocompleteMatch match,
      WindowOpenDisposition disposition,
      base::TimeTicks match_selection_timestamp,
      OmniboxClient::ExtensionControlledDialogResult dialog_result);

  searchbox::mojom::AutocompleteResultPtr CreateAutocompleteResult(
      int32_t query_id,
      const std::u16string& input,
      const AutocompleteResult& result,
      bookmarks::BookmarkModel* bookmark_model,
      const PrefService* prefs,
      const TemplateURLService* turl_service) const;
  base::flat_map<int32_t, searchbox::mojom::SuggestionGroupPtr>
  CreateSuggestionGroupsMap(
      const AutocompleteResult& result,
      const PrefService* prefs,
      const omnibox::GroupConfigMap& suggestion_groups_map) const;
  std::vector<searchbox::mojom::AutocompleteMatchPtr> CreateAutocompleteMatches(
      const AutocompleteResult& result,
      bookmarks::BookmarkModel* bookmark_model,
      const omnibox::GroupConfigMap& suggestion_groups_map,
      const TemplateURLService* turl_service) const;
  virtual std::optional<searchbox::mojom::AutocompleteMatchPtr>
  CreateAutocompleteMatch(const AutocompleteMatch& match,
                          size_t line,
                          bookmarks::BookmarkModel* bookmark_model,
                          const omnibox::GroupConfigMap& suggestion_groups_map,
                          const TemplateURLService* turl_service) const;
  virtual WindowOpenDisposition ComputeWindowOpenDisposition(
      uint8_t mouse_button,
      bool alt_key,
      bool ctrl_key,
      bool meta_key,
      bool shift_key,
      bool via_keyboard);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_SEARCHBOX_HANDLER_H_
