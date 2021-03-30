// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/common/search/search.mojom.h"
#include "chrome/renderer/instant_restricted_id_cache.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "url/gurl.h"

// The renderer-side implementation of the embeddedSearch API (see
// https://www.chromium.org/embeddedsearch).
class SearchBox : public content::RenderFrameObserver,
                  public content::RenderFrameObserverTracker<SearchBox>,
                  public search::mojom::EmbeddedSearchClient {
 public:
  // Helper class for GenerateImageURLFromTransientURL() to adapt SearchBox's
  // instance, thereby allow mocking for unit tests.
  class IconURLHelper {
   public:
    IconURLHelper();
    virtual ~IconURLHelper();
    // Retruns view id for validating icon URL.
    virtual int GetViewID() const = 0;
    // Returns the page URL string for |rid|, or empty string for invalid |rid|.
    virtual std::string GetURLStringFromRestrictedID(InstantRestrictedID rid)
        const = 0;
  };

  explicit SearchBox(content::RenderFrame* render_frame);
  ~SearchBox() override;

  // Sends LogEvent to the browser.
  void LogEvent(NTPLoggingEventType event);

  // Sends LogSuggestionEventWithValue to the browser.
  void LogSuggestionEventWithValue(NTPSuggestionsLoggingEventType event,
                                   int data);

  // Sends LogMostVisitedImpression to the browser.
  void LogMostVisitedImpression(const ntp_tiles::NTPTileImpression& impression);

  // Sends LogMostVisitedNavigation to the browser.
  void LogMostVisitedNavigation(const ntp_tiles::NTPTileImpression& impression);

  // Sends DeleteMostVisitedItem to the browser.
  void DeleteMostVisitedItem(InstantRestrictedID most_visited_item_id);

  // Generates the image URL of the most visited item favicon specified by
  // |transient_url|. If |transient_url| is valid, |url| is set with a
  // translated URL. Otherwise, |url| is set the the default favicon
  // ("chrome-search://favicon/").
  //
  // Valid forms of |transient_url|:
  //    chrome-search://favicon/<view_id>/<restricted_id>
  //    chrome-search://favicon/<favicon_parameters>/<view_id>/<restricted_id>
  //
  // We do this to prevent search providers from abusing image URLs and deduce
  // whether the user has visited a particular page. For example, if
  // "chrome-search://favicon/http://www.secretsite.com" is accessible, then
  // the search provider can use its return code to determine whether the user
  // has visited "http://www.secretsite.com". Therefore we require search
  // providers to specify URL by "<view_id>/<restricted_id>". We then translate
  // this to the original |url|, and pass the request to the proper endpoint.
  void GenerateImageURLFromTransientURL(const GURL& transient_url,
                                        GURL* url) const;

  // Returns the latest most visited items sent by the browser.
  void GetMostVisitedItems(
      std::vector<InstantMostVisitedItemIDPair>* items) const;

  bool AreMostVisitedItemsAvailable() const;

  // If the |most_visited_item_id| is found in the cache, sets |item| to it
  // and returns true.
  bool GetMostVisitedItemWithID(InstantRestrictedID most_visited_item_id,
                                InstantMostVisitedItem* item) const;

  // Sends PasteAndOpenDropdown to the browser.
  void Paste(const std::u16string& text);

  // Will return null if the theme info hasn't been set yet.
  const NtpTheme* GetNtpTheme() const;

  // Sends FocusOmnibox(OMNIBOX_FOCUS_INVISIBLE) to the browser.
  void StartCapturingKeyStrokes();

  // Sends FocusOmnibox(OMNIBOX_FOCUS_NONE) to the browser.
  void StopCapturingKeyStrokes();

  // Sends UndoAllMostVisitedDeletions to the browser.
  void UndoAllMostVisitedDeletions();

  // Sends UndoMostVisitedDeletion to the browser.
  void UndoMostVisitedDeletion(InstantRestrictedID most_visited_item_id);

  // Returns true if the most visited items are custom links.
  bool IsCustomLinks() const;

  // Returns true if most visited is enabled instead of custom links.
  bool IsUsingMostVisited() const;

  // Returns true if the shortcuts are visible and not hidden by the user.
  bool AreShortcutsVisible() const;

  // Sends AddCustomLink to the browser.
  void AddCustomLink(const GURL& url, const std::string& title);

  // Sends UpdateCustomLink to the browser.
  void UpdateCustomLink(InstantRestrictedID link_id,
                        const GURL& new_url,
                        const std::string& new_title);

  // Sends ReorderCustomLink to the browser.
  void ReorderCustomLink(InstantRestrictedID link_id, int new_pos);

  // Sends DeleteCustomLink to the browser.
  void DeleteCustomLink(InstantRestrictedID most_visited_item_id);

  // Sends UndoCustomLinkAction to the browser.
  void UndoCustomLinkAction();

  // Sends ResetCustomLinks to the browser.
  void ResetCustomLinks();

  // Sends ToggleMostVisitedOrCustomLinks to the browser.
  void ToggleMostVisitedOrCustomLinks();

  // Sends ToggleShortcutsVisibility to the browser.
  void ToggleShortcutsVisibility(bool do_notify);

  // Attempts to fix obviously invalid URLs. Uses the "https" scheme unless
  // otherwise specified. Returns the fixed URL if valid, otherwise returns an
  // empty string.
  std::string FixupAndValidateUrl(const std::string& url) const;

  // Updates the NTP custom background preferences, sometimes this includes
  // image attributions.
  void SetCustomBackgroundInfo(const GURL& background_url,
                               const std::string& attribution_line_1,
                               const std::string& attribution_line_2,
                               const GURL& action_url,
                               const std::string& collection_id);

  // Let the user select a local file for the NTP background.
  void SelectLocalBackgroundImage();

  // Add a search suggestion task id to the blocklist.
  void BlocklistSearchSuggestion(int task_version, long task_id);

  // Add a search suggestion task id and hash to the blocklist.
  void BlocklistSearchSuggestionWithHash(int task_version,
                                         long task_id,
                                         const std::vector<uint8_t>& hash);

  // A suggestion collected, issue a new request with the suggestion
  // temporarily added to the blocklist.
  void SearchSuggestionSelected(int task_version,
                                long task_id,
                                const std::vector<uint8_t>& hash);

  // Opts the user out of receiving search suggestions.
  void OptOutOfSearchSuggestions();

  // Applies the default theme.
  void ApplyDefaultTheme();

  // Applies autogenerated theme for the given color.
  void ApplyAutogeneratedTheme(SkColor color);

  // Reverts applied theme changes.
  void RevertThemeChanges();

  // Confirms applied theme changes.
  void ConfirmThemeChanges();

  // Queries the autocomplete backend for realbox results for |input| as a
  // search term. |prevent_inline_autocomplete| is true if the result set should
  // not require inline autocomplete for the default match. Handled by
  // |QueryAutocompleteResult|.
  void QueryAutocomplete(const std::u16string& input,
                         bool prevent_inline_autocomplete);

  // Deletes |AutocompleteMatch| by index of the result.
  void DeleteAutocompleteMatch(uint8_t line);

  // Cancels the current autocomplete query. Clears the result set if
  // |clear_result| is true.
  void StopAutocomplete(bool clear_result);

  // Logs the time it took in milliseconds since the first character (in a
  // series of characters) was typed until Autocomplete results were painted.
  void LogCharTypedToRepaintLatency(uint32_t latency_ms);

  // Called when a user dismisses a promo.
  void BlocklistPromo(const std::string& promo_id);

  // Handles navigation to the chrome://extensions page by calling the browser
  // to do the navigation.
  void OpenExtensionsPage(double button,
                          bool alt_key,
                          bool ctrl_key,
                          bool meta_key,
                          bool shift_key);

  // Handles navigation to privileged (i.e. chrome://) URLs by calling the
  // browser to do the navigation.
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             double time_elapsed_since_last_focus,
                             double button,
                             bool alt_key,
                             bool ctrl_key,
                             bool meta_key,
                             bool shift_key);

  // Tells the browser to allow suggestions with the given suggestion group ID
  // to appear in the results if they currently are not allowed to or to prevent
  // them from appearing in the results if they are currently permitted to.
  void ToggleSuggestionGroupIdVisibility(int32_t suggestion_group_id);

  bool is_focused() const { return is_focused_; }
  bool is_input_in_progress() const { return is_input_in_progress_; }
  bool is_key_capture_enabled() const { return is_key_capture_enabled_; }

 private:
  // Overridden from content::RenderFrameObserver:
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void OnDestruct() override;

  // Overridden from search::mojom::EmbeddedSearchClient:
  void AutocompleteResultChanged(
      search::mojom::AutocompleteResultPtr result) override;
  void AutocompleteMatchImageAvailable(uint32_t match_index,
                                       const std::string& image_url,
                                       const std::string& data_url) override;
  void SetPageSequenceNumber(int page_seq_no) override;
  void FocusChanged(OmniboxFocusState new_focus_state,
                    OmniboxFocusChangeReason reason) override;
  void MostVisitedInfoChanged(
      const InstantMostVisitedInfo& most_visited_info) override;
  void SetInputInProgress(bool input_in_progress) override;
  void ThemeChanged(const NtpTheme& theme) override;
  void LocalBackgroundSelected() override;

  void AddCustomLinkResult(bool success);
  void UpdateCustomLinkResult(bool success);
  void DeleteCustomLinkResult(bool success);

  // Returns the URL of the Most Visited item specified by the |item_id|.
  GURL GetURLForMostVisitedItem(InstantRestrictedID item_id) const;

  // The connection to the EmbeddedSearch service in the browser process.
  mojo::AssociatedRemote<search::mojom::EmbeddedSearch>
      embedded_search_service_;
  mojo::AssociatedReceiver<search::mojom::EmbeddedSearchClient> receiver_{this};

  // Whether it's legal to execute JavaScript in |render_frame()|.
  // This class may want to execute JS in response to IPCs (via the
  // SearchBoxExtension::Dispatch* methods). However, for cross-process
  // navigations, a "provisional frame" is created at first, and it's illegal
  // to execute any JS in it before it is actually swapped in, i.e. before the
  // navigation has committed. So this only gets set to true in
  // RenderFrameObserver::DidCommitProvisionalLoad. See crbug.com/765101.
  // Note: If crbug.com/794942 ever gets resolved, then it might be possible to
  // move the mojo connection code from the ctor to DidCommitProvisionalLoad and
  // avoid this bool.
  bool can_run_js_in_renderframe_;

  // The Instant state.
  int page_seq_no_;
  bool is_focused_;
  bool is_input_in_progress_;
  bool is_key_capture_enabled_;
  InstantRestrictedIDCache<InstantMostVisitedItem> most_visited_items_cache_;
  // Use |most_visited_items_cache_| instead of |most_visited_info_.items| when
  // comparing most visited items.
  InstantMostVisitedInfo most_visited_info_;
  bool has_received_most_visited_;
  base::Optional<NtpTheme> theme_;

  base::WeakPtrFactory<SearchBox> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
