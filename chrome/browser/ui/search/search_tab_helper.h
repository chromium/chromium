// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/search/chrome_colors/chrome_colors_service.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_ANDROID)
#error "Instant is only used on desktop";
#endif

namespace content {
class WebContents;
struct LoadCommittedDetails;
}

namespace gfx {
class Image;
}

class AutocompleteController;
class GURL;
class InstantService;
class Profile;
class SearchIPCRouterTest;
class SearchSuggestService;
class SkBitmap;

// This is the browser-side, per-tab implementation of the embeddedSearch API
// (see https://www.chromium.org/embeddedsearch).
class SearchTabHelper : public content::WebContentsObserver,
                        public content::WebContentsUserData<SearchTabHelper>,
                        public InstantServiceObserver,
                        public SearchIPCRouter::Delegate,
                        public ui::SelectFileDialog::Listener,
                        public AutocompleteController::Observer,
                        public OmniboxTabHelper::Observer {
 public:
  ~SearchTabHelper() override;

  // Called when the tab corresponding to |this| instance is activated.
  void OnTabActivated();

  // Called when the tab corresponding to |this| instance is deactivated.
  void OnTabDeactivated();

  // Called when the tab corresponding to |this| instance is closing.
  void OnTabClosing();

  SearchIPCRouter& ipc_router_for_testing() { return ipc_router_; }

  // Returns an equivalent SVG for the given Omnibox vector icon for use in the
  // NTP Realbox.
  static std::string AutocompleteMatchVectorIconToResourceName(
      const gfx::VectorIcon& icon);

 private:
  friend class content::WebContentsUserData<SearchTabHelper>;
  friend class SearchIPCRouterTest;

  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           FileSelectedUpdatesLastSelectedDirectory);

  explicit SearchTabHelper(content::WebContents* web_contents);

  // Overridden from contents::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {}
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  // Overridden from SearchIPCRouter::Delegate:
  void FocusOmnibox(bool focus) override;
  void OnDeleteMostVisitedItem(const GURL& url) override;
  void OnUndoMostVisitedDeletion(const GURL& url) override;
  void OnUndoAllMostVisitedDeletions() override;
  bool OnAddCustomLink(const GURL& url, const std::string& title) override;
  bool OnUpdateCustomLink(const GURL& url,
                          const GURL& new_url,
                          const std::string& new_title) override;
  bool OnReorderCustomLink(const GURL& url, int new_pos) override;
  bool OnDeleteCustomLink(const GURL& url) override;
  void OnUndoCustomLinkAction() override;
  void OnResetCustomLinks() override;
  void OnToggleMostVisitedOrCustomLinks() override;
  void OnToggleShortcutsVisibility(bool do_notify) override;
  void OnLogEvent(NTPLoggingEventType event, base::TimeDelta time) override;
  void OnLogSuggestionEventWithValue(NTPSuggestionsLoggingEventType event,
                                     int data,
                                     base::TimeDelta time) override;
  void OnLogMostVisitedImpression(
      const ntp_tiles::NTPTileImpression& impression) override;
  void OnLogMostVisitedNavigation(
      const ntp_tiles::NTPTileImpression& impression) override;
  void PasteIntoOmnibox(const base::string16& text) override;
  void OnSetCustomBackgroundInfo(const GURL& background_url,
                                 const std::string& attribution_line_1,
                                 const std::string& attribution_line_2,
                                 const GURL& action_url,
                                 const std::string& collection_id) override;
  void OnSelectLocalBackgroundImage() override;
  void OnBlocklistSearchSuggestion(int task_version, long task_id) override;
  void OnBlocklistSearchSuggestionWithHash(int task_version,
                                           long task_id,
                                           const uint8_t hash[4]) override;
  void OnSearchSuggestionSelected(int task_version,
                                  long task_id,
                                  const uint8_t hash[4]) override;
  void OnOptOutOfSearchSuggestions() override;
  void OnApplyDefaultTheme() override;
  void OnApplyAutogeneratedTheme(SkColor color) override;
  void OnRevertThemeChanges() override;
  void OnConfirmThemeChanges() override;
  void QueryAutocomplete(const base::string16& input,
                         bool prevent_inline_autocomplete) override;
  void DeleteAutocompleteMatch(uint8_t line) override;
  void StopAutocomplete(bool clear_result) override;
  void ToggleSuggestionGroupIdVisibility(int32_t suggestion_group_id) override;
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

  // Overridden from InstantServiceObserver:
  void NtpThemeChanged(const NtpTheme& theme) override;
  void MostVisitedInfoChanged(
      const InstantMostVisitedInfo& most_visited_info) override;

  // Overridden from SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  // Overridden from AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // Overridden from OmniboxTabHelper::Observer:
  void OnOmniboxInputStateChanged() override;
  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override;

  void OnBitmapFetched(int match_index,
                       const std::string& image_url,
                       const SkBitmap& bitmap);

  void OnFaviconFetched(int match_index,
                        const std::string& page_url,
                        const gfx::Image& favicon);

  Profile* profile() const;

  // Returns whether input is in progress, i.e. if the omnibox has focus and the
  // active tab is in mode SEARCH_SUGGESTIONS.
  bool IsInputInProgress() const;

  // Called when a user confirms deleting an autocomplete match. Note: might be
  // called synchronously with accepted = true if this feature is disabled
  // (which defaults the behavior to silent deletions).
  void OnDeleteAutocompleteMatchConfirm(
      uint8_t line,
      bool accepted);

  content::WebContents* web_contents_;

  SearchIPCRouter ipc_router_;

  InstantService* instant_service_;

  SearchSuggestService* search_suggest_service_;

  bool is_setting_title_ = false;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  chrome_colors::ChromeColorsService* chrome_colors_service_;

  std::unique_ptr<AutocompleteController> autocomplete_controller_;
  base::TimeTicks time_of_first_autocomplete_query_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  FaviconCache favicon_cache_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<SearchTabHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchTabHelper);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
