// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/common/search/omnibox.mojom.h"
#include "chrome/common/search/search.mojom.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

#if defined(OS_ANDROID)
#error "Instant is only used on desktop";
#endif

class GURL;

namespace content {
class WebContents;
}

class SearchIPCRouterTest;

// SearchIPCRouter is responsible for receiving and sending IPC messages between
// the browser and the Instant page.
class SearchIPCRouter : public content::WebContentsObserver,
                        public search::mojom::EmbeddedSearch {
 public:
  // SearchIPCRouter calls its delegate in response to messages received from
  // the page.
  class Delegate {
   public:
    // Called when the page wants the omnibox to be focused.
    virtual void FocusOmnibox(bool focus) = 0;

    // Called when the EmbeddedSearch wants to delete a Most Visited item.
    virtual void OnDeleteMostVisitedItem(const GURL& url) = 0;

    // Called when the EmbeddedSearch wants to undo a Most Visited deletion.
    virtual void OnUndoMostVisitedDeletion(const GURL& url) = 0;

    // Called when the EmbeddedSearch wants to undo all Most Visited deletions.
    virtual void OnUndoAllMostVisitedDeletions() = 0;

    // Called when the EmbeddedSearch wants to add a custom link.
    virtual bool OnAddCustomLink(const GURL& url, const std::string& title) = 0;

    // Called when the EmbeddedSearch wants to update a custom link.
    virtual bool OnUpdateCustomLink(const GURL& url,
                                    const GURL& new_url,
                                    const std::string& new_title) = 0;

    // Called when the EmbeddedSearch wants to reorder a custom link.
    virtual bool OnReorderCustomLink(const GURL& url, int new_pos) = 0;

    // Called when the EmbeddedSearch wants to delete a custom link.
    virtual bool OnDeleteCustomLink(const GURL& url) = 0;

    // Called when the EmbeddedSearch wants to undo the previous custom link
    // action.
    virtual void OnUndoCustomLinkAction() = 0;

    // Called when the EmbeddedSearch wants to delete all custom links and
    // use Most Visited sites instead.
    virtual void OnResetCustomLinks() = 0;

    // Called when the EmbeddedSearch wants to switch between custom links and
    // Most Visited.
    virtual void OnToggleMostVisitedOrCustomLinks() = 0;

    // Called when the EmbeddedSearch wants to toggle visibility of the
    // shortcuts.
    virtual void OnToggleShortcutsVisibility(bool do_notify) = 0;

    // Called to signal that an event has occurred on the New Tab Page at a
    // particular time since navigation start.
    virtual void OnLogEvent(NTPLoggingEventType event,
                            base::TimeDelta time) = 0;

    // Called to signal that an event has occurred on the New Tab Page at a
    // particular time since navigation start, and provide an int value.
    virtual void OnLogSuggestionEventWithValue(
        NTPSuggestionsLoggingEventType event,
        int data,
        base::TimeDelta time) = 0;

    // Called to log an impression from a given provider on the New Tab Page.
    virtual void OnLogMostVisitedImpression(
        const ntp_tiles::NTPTileImpression& impression) = 0;

    // Called to log a navigation from a given provider on the New Tab Page.
    virtual void OnLogMostVisitedNavigation(
        const ntp_tiles::NTPTileImpression& impression) = 0;

    // Called when the page wants to paste the |text| (or the clipboard contents
    // if the |text| is empty) into the omnibox.
    virtual void PasteIntoOmnibox(const base::string16& text) = 0;

    // Called when a custom background is configured on the NTP.
    // background_url: Url of the background image.
    // attribution_line_1: First attribution line for the image.
    // attribution_line_2: Second attribution line for the image.
    // action_url: Url to learn more about the backgrounds image.
    // collection_id: Id of the collection that was selected.
    virtual void OnSetCustomBackgroundInfo(
        const GURL& background_url,
        const std::string& attribution_line_1,
        const std::string& attribution_line_2,
        const GURL& action_url,
        const std::string& collection_id) = 0;

    // Called to open the file select dialog for selecting a
    // NTP background image.
    virtual void OnSelectLocalBackgroundImage() = 0;

    // Called when a search suggestion is blocklisted on the local NTP.
    virtual void OnBlocklistSearchSuggestion(int task_version,
                                             long task_id) = 0;

    // Called when a search suggestion is blocklisted on the local NTP and a
    // hash is provided.
    virtual void OnBlocklistSearchSuggestionWithHash(int task_version,
                                                     long task_id,
                                                     const uint8_t hash[4]) = 0;

    // Called when a search suggestion is selected on the local NTP.
    virtual void OnSearchSuggestionSelected(int task_version,
                                            long task_id,
                                            const uint8_t hash[4]) = 0;

    // Called when a user selected to completely opt out of NTP search
    // suggestions.
    virtual void OnOptOutOfSearchSuggestions() = 0;

    // Called when users selects the default theme to apply.
    virtual void OnApplyDefaultTheme() = 0;

    // Called when users selects an autogenerated theme to apply.
    virtual void OnApplyAutogeneratedTheme(SkColor color) = 0;

    // Called when users cancels applied theme changes.
    virtual void OnRevertThemeChanges() = 0;

    // Called when user confirms applied theme changes.
    virtual void OnConfirmThemeChanges() = 0;

    virtual void QueryAutocomplete(const base::string16& input,
                                   bool prevent_inline_autocomplete) = 0;

    virtual void StopAutocomplete(bool clear_result) = 0;

    virtual void LogCharTypedToRepaintLatency(uint32_t latency_ms) = 0;

    virtual void BlocklistPromo(const std::string& promo_id) = 0;

    virtual void OpenExtensionsPage(double button,
                                    bool alt_key,
                                    bool ctrl_key,
                                    bool meta_key,
                                    bool shift_key) = 0;

    virtual void OpenAutocompleteMatch(uint8_t line,
                                       const GURL& url,
                                       bool are_matches_showing,
                                       double time_elapsed_since_last_focus,
                                       double button,
                                       bool alt_key,
                                       bool ctrl_key,
                                       bool meta_key,
                                       bool shift_key) = 0;

    virtual void DeleteAutocompleteMatch(uint8_t line) = 0;

    virtual void ToggleSuggestionGroupIdVisibility(
        int32_t suggestion_group_id) = 0;
  };

  // An interface to be implemented by consumers of SearchIPCRouter objects to
  // decide whether to process the message received from the page, and vice
  // versa (decide whether to send messages to the page).
  class Policy {
   public:
    virtual ~Policy() {}

    // SearchIPCRouter calls these functions before sending/receiving messages
    // to/from the page.
    virtual bool ShouldProcessFocusOmnibox(bool is_active_tab) = 0;
    virtual bool ShouldProcessDeleteMostVisitedItem() = 0;
    virtual bool ShouldProcessUndoMostVisitedDeletion() = 0;
    virtual bool ShouldProcessUndoAllMostVisitedDeletions() = 0;
    virtual bool ShouldProcessAddCustomLink() = 0;
    virtual bool ShouldProcessUpdateCustomLink() = 0;
    virtual bool ShouldProcessReorderCustomLink() = 0;
    virtual bool ShouldProcessDeleteCustomLink() = 0;
    virtual bool ShouldProcessUndoCustomLinkAction() = 0;
    virtual bool ShouldProcessResetCustomLinks() = 0;
    virtual bool ShouldProcessToggleMostVisitedOrCustomLinks() = 0;
    virtual bool ShouldProcessToggleShortcutsVisibility() = 0;
    virtual bool ShouldProcessLogEvent() = 0;
    virtual bool ShouldProcessLogSuggestionEventWithValue() = 0;
    virtual bool ShouldProcessPasteIntoOmnibox(bool is_active_tab) = 0;
    virtual bool ShouldSendSetInputInProgress(bool is_active_tab) = 0;
    virtual bool ShouldSendOmniboxFocusChanged() = 0;
    virtual bool ShouldSendMostVisitedInfo() = 0;
    virtual bool ShouldSendNtpTheme() = 0;
    virtual bool ShouldSendLocalBackgroundSelected() = 0;
    virtual bool ShouldProcessSetCustomBackgroundInfo() = 0;
    virtual bool ShouldProcessSelectLocalBackgroundImage() = 0;
    virtual bool ShouldProcessBlocklistSearchSuggestion() = 0;
    virtual bool ShouldProcessBlocklistSearchSuggestionWithHash() = 0;
    virtual bool ShouldProcessSearchSuggestionSelected() = 0;
    virtual bool ShouldProcessOptOutOfSearchSuggestions() = 0;
    virtual bool ShouldProcessThemeChangeMessages() = 0;
    virtual bool ShouldProcessAutocompleteResultChanged(bool is_active_tab) = 0;
    virtual bool ShouldProcessAutocompleteMatchImageAvailable(
        bool is_active_tab) = 0;
    virtual bool ShouldProcessQueryAutocomplete(bool is_active_tab) = 0;
    virtual bool ShouldProcessStopAutocomplete() = 0;
    virtual bool ShouldProcessLogCharTypedToRepaintLatency() = 0;
    virtual bool ShouldProcessBlocklistPromo() = 0;
    virtual bool ShouldProcessOpenExtensionsPage() = 0;
    virtual bool ShouldProcessOpenAutocompleteMatch(bool is_active_tab) = 0;
    virtual bool ShouldProcessDeleteAutocompleteMatch() = 0;
    virtual bool ShouldProcessToggleSuggestionGroupIdVisibility() = 0;
  };

  // Creates search::mojom::EmbeddedSearchClient connections on request.
  class EmbeddedSearchClientFactory {
   public:
    EmbeddedSearchClientFactory() = default;
    virtual ~EmbeddedSearchClientFactory() = default;

    // The returned pointer is owned by the factory.
    virtual search::mojom::EmbeddedSearchClient* GetEmbeddedSearchClient() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(EmbeddedSearchClientFactory);
  };

  SearchIPCRouter(content::WebContents* web_contents,
                  Delegate* delegate,
                  std::unique_ptr<Policy> policy);
  ~SearchIPCRouter() override;

  // Updates the renderer with the autocomplete results.
  void AutocompleteResultChanged(search::mojom::AutocompleteResultPtr result);

  // Updates the renderer with the given autocomplete match's image data.
  void AutocompleteMatchImageAvailable(uint32_t match_index,
                                       const std::string& image_url,
                                       const std::string& data_url);

  // Tells the SearchIPCRouter that a new page in an Instant process committed.
  void OnNavigationEntryCommitted();

  // Tells the page that user input started or stopped.
  void SetInputInProgress(bool input_in_progress);

  // Tells the page that the omnibox focus has changed.
  void OmniboxFocusChanged(OmniboxFocusState state,
                           OmniboxFocusChangeReason reason);

  // Tells the renderer about the most visited items.
  void SendMostVisitedInfo(const InstantMostVisitedInfo& most_visited_info);

  // Tells the renderer about the current theme background.
  void SendNtpTheme(const NtpTheme& theme);

  // Tells the renderer that "Done" was clicked on the file selection dialog for
  // uploading a image to use as the NTP background.
  void SendLocalBackgroundSelected();

  // Called when the tab corresponding to |this| instance is activated.
  void OnTabActivated();

  // Called when the tab corresponding to |this| instance is deactivated.
  void OnTabDeactivated();

  // search::mojom::EmbeddedSearch:
  void FocusOmnibox(int page_id, bool focus) override;
  void DeleteMostVisitedItem(int page_seq_no, const GURL& url) override;
  void UndoMostVisitedDeletion(int page_seq_no, const GURL& url) override;
  void UndoAllMostVisitedDeletions(int page_seq_no) override;
  void AddCustomLink(int page_seq_no,
                     const GURL& url,
                     const std::string& title,
                     AddCustomLinkCallback callback) override;
  void UpdateCustomLink(int page_seq_no,
                        const GURL& url,
                        const GURL& new_url,
                        const std::string& new_title,
                        UpdateCustomLinkCallback callback) override;
  void ReorderCustomLink(int page_seq_no,
                         const GURL& url,
                         int new_pos) override;
  void DeleteCustomLink(int page_seq_no,
                        const GURL& url,
                        DeleteCustomLinkCallback callback) override;
  void UndoCustomLinkAction(int page_seq_no) override;
  void ResetCustomLinks(int page_seq_no) override;
  void ToggleMostVisitedOrCustomLinks(int page_seq_no) override;
  void ToggleShortcutsVisibility(int page_seq_no, bool do_notify) override;
  void LogEvent(int page_seq_no,
                NTPLoggingEventType event,
                base::TimeDelta time) override;
  void LogSuggestionEventWithValue(int page_seq_no,
                                   NTPSuggestionsLoggingEventType event,
                                   int data,
                                   base::TimeDelta time) override;
  void LogMostVisitedImpression(
      int page_seq_no,
      const ntp_tiles::NTPTileImpression& impression) override;
  void LogMostVisitedNavigation(
      int page_seq_no,
      const ntp_tiles::NTPTileImpression& impression) override;
  void PasteAndOpenDropdown(int page_seq_no,
                            const base::string16& text) override;
  void SetCustomBackgroundInfo(const GURL& background_url,
                               const std::string& attribution_line_1,
                               const std::string& attribution_line_2,
                               const GURL& action_url,
                               const std::string& collection_id) override;
  void SelectLocalBackgroundImage() override;
  void BlocklistSearchSuggestion(int32_t task_version,
                                 int64_t task_id) override;
  void BlocklistSearchSuggestionWithHash(
      int32_t task_version,
      int64_t task_id,
      const std::vector<uint8_t>& hash) override;
  void SearchSuggestionSelected(int32_t task_version,
                                int64_t task_id,
                                const std::vector<uint8_t>& hash) override;
  void OptOutOfSearchSuggestions() override;
  void ApplyDefaultTheme() override;
  void ApplyAutogeneratedTheme(SkColor color) override;
  void RevertThemeChanges() override;
  void ConfirmThemeChanges() override;
  void QueryAutocomplete(const base::string16& input,
                         bool prevent_inline_autocomplete) override;
  void StopAutocomplete(bool clear_result) override;
  void LogCharTypedToRepaintLatency(uint32_t latency_ms) override;
  void BlocklistPromo(const std::string& promo_id) override;
  void OpenExtensionsPage(double button,
                          bool alt_key,
                          bool ctrl_key,
                          bool meta_key,
                          bool shift_key) override;
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             double time_elapsed_since_last_focus,
                             double button,
                             bool alt_key,
                             bool ctrl_key,
                             bool meta_key,
                             bool shift_key) override;
  void DeleteAutocompleteMatch(uint8_t line) override;
  void ToggleSuggestionGroupIdVisibility(int32_t suggestion_group_id) override;
  void set_embedded_search_client_factory_for_testing(
      std::unique_ptr<EmbeddedSearchClientFactory> factory) {
    embedded_search_client_factory_ = std::move(factory);
  }

 private:
  friend class SearchIPCRouterPolicyTest;
  friend class SearchIPCRouterTest;
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, HandleTabChangedEvents);

  // Used by unit tests to set a fake delegate.
  void set_delegate_for_testing(Delegate* delegate);

  // Used by unit tests.
  void set_policy_for_testing(std::unique_ptr<Policy> policy);

  // Used by unit tests.
  Policy* policy_for_testing() const { return policy_.get(); }

  // Used by unit tests.
  int page_seq_no_for_testing() const { return commit_counter_; }

  search::mojom::EmbeddedSearchClient* embedded_search_client() {
    return embedded_search_client_factory_->GetEmbeddedSearchClient();
  }

  Delegate* delegate_;
  std::unique_ptr<Policy> policy_;

  // Holds the number of main frame commits executed in this tab. Used by the
  // SearchIPCRouter to ensure that delayed IPC replies are ignored.
  int commit_counter_;

  // Set to true, when the tab corresponding to |this| instance is active.
  bool is_active_tab_;

  // Receiver for the connected main frame. We only allow one frame to connect
  // at the moment, but this could be extended to a map of connected frames, if
  // desired.
  mojo::AssociatedReceiver<search::mojom::EmbeddedSearch> receiver_{this};

  std::unique_ptr<EmbeddedSearchClientFactory> embedded_search_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchIPCRouter);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_
