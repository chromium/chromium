// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/common/search.mojom.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/renderer/instant_restricted_id_cache.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "url/gurl.h"

// The renderer-side implementation of the embeddedSearch API (see
// https://www.chromium.org/embeddedsearch).
class SearchBox : public content::RenderFrameObserver,
                  public content::RenderFrameObserverTracker<SearchBox>,
                  public chrome::mojom::EmbeddedSearchClient {
 public:
  enum ImageSourceType {
    NONE = -1,
    FAVICON,
    THUMB
  };

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

  // Sends LogMostVisitedImpression to the browser.
  void LogMostVisitedImpression(const ntp_tiles::NTPTileImpression& impression);

  // Sends LogMostVisitedNavigation to the browser.
  void LogMostVisitedNavigation(const ntp_tiles::NTPTileImpression& impression);

  // Sends ChromeIdentityCheck to the browser.
  void CheckIsUserSignedInToChromeAs(const base::string16& identity);

  // Sends HistorySyncCheck to the browser.
  void CheckIsUserSyncingHistory();

  // Sends DeleteMostVisitedItem to the browser.
  void DeleteMostVisitedItem(InstantRestrictedID most_visited_item_id);

  // Generates the image URL of |type| for the most visited item specified in
  // |transient_url|. If |transient_url| is valid, |url| with a translated URL
  // and returns true.  Otherwise it depends on |type|:
  // - FAVICON: Returns true and renders an URL to display the default favicon.
  //
  // For |type| == FAVICON, valid forms of |transient_url|:
  //    chrome-search://favicon/<view_id>/<restricted_id>
  //    chrome-search://favicon/<favicon_parameters>/<view_id>/<restricted_id>
  //
  // For |type| == THUMB, valid form of |transient_url|:
  //    chrome-search://thumb/<render_view_id>/<most_visited_item_id>
  //
  // We do this to prevent search providers from abusing image URLs and deduce
  // whether the user has visited a particular page. For example, if
  // "chrome-search://favicon/http://www.secretsite.com" is accessible, then
  // the search provider can use its return code to determine whether the user
  // has visited "http://www.secretsite.com". Therefore we require search
  // providers to specify URL by "<view_id>/<restricted_id>". We then translate
  // this to the original |url|, and pass the request to the proper endpoint.
  bool GenerateImageURLFromTransientURL(const GURL& transient_url,
                                        ImageSourceType type,
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
  void Paste(const base::string16& text);

  const ThemeBackgroundInfo& GetThemeBackgroundInfo() const;

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

  // Sends AddCustomLink to the browser.
  void AddCustomLink(const GURL& url, const std::string& title);

  // Sends UpdateCustomLink to the browser.
  void UpdateCustomLink(InstantRestrictedID link_id,
                        const GURL& new_url,
                        const std::string& new_title);

  // Sends DeleteCustomLink to the browser.
  void DeleteCustomLink(InstantRestrictedID most_visited_item_id);

  // Sends UndoCustomLinkAction to the browser.
  void UndoCustomLinkAction();

  // Sends ResetCustomLinks to the browser.
  void ResetCustomLinks();

  // Attempts to fix obviously invalid URLs. Uses the "https" scheme unless
  // otherwise specified and, if so, checks if the default scheme can resolve.
  // Returns the fixed URL if valid, otherwise returns an empty string.
  std::string FixupAndValidateUrl(const std::string& url);

  // Updates the NTP custom background preferences, sometimes this includes
  // image attributions.
  void SetCustomBackgroundURL(const GURL& background_url);
  void SetCustomBackgroundURLWithAttributions(
      const GURL& background_url,
      const std::string& attribution_line_1,
      const std::string& attribution_line_2,
      const GURL& action_url);

  // Let the user select a local file for the NTP background.
  void SelectLocalBackgroundImage();

  bool is_focused() const { return is_focused_; }
  bool is_input_in_progress() const { return is_input_in_progress_; }
  bool is_key_capture_enabled() const { return is_key_capture_enabled_; }

 private:
  // Overridden from content::RenderFrameObserver:
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void OnDestruct() override;

  // Overridden from chrome::mojom::EmbeddedSearchClient:
  void SetPageSequenceNumber(int page_seq_no) override;
  void FocusChanged(OmniboxFocusState new_focus_state,
                    OmniboxFocusChangeReason reason) override;
  void MostVisitedChanged(const std::vector<InstantMostVisitedItem>& items,
                          bool is_custom_links) override;
  void SetInputInProgress(bool input_in_progress) override;
  void ThemeChanged(const ThemeBackgroundInfo& theme_info) override;

  void HistorySyncCheckResult(bool sync_history);
  void ChromeIdentityCheckResult(const base::string16& identity,
                                 bool identity_match);
  void AddCustomLinkResult(bool success);
  void UpdateCustomLinkResult(bool success);
  void DeleteCustomLinkResult(bool success);
  void DoesUrlResolveResult(bool resolves, bool timeout) const;

  // Returns the URL of the Most Visited item specified by the |item_id|.
  GURL GetURLForMostVisitedItem(InstantRestrictedID item_id) const;

  // The connection to the EmbeddedSearch service in the browser process.
  chrome::mojom::EmbeddedSearchAssociatedPtr embedded_search_service_;
  mojo::AssociatedBinding<chrome::mojom::EmbeddedSearchClient> binding_;

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
  bool has_received_most_visited_;
  // True if the most visited items are custom links.
  bool is_custom_links_ = false;
  ThemeBackgroundInfo theme_info_;

  base::WeakPtrFactory<SearchBox> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
