// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(IS_ANDROID)
#error "Instant is only used on desktop";
#endif

namespace content {
class WebContents;
struct LoadCommittedDetails;
}

namespace gfx {
class Image;
}

class GURL;
class InstantService;
class Profile;
class SearchIPCRouterTest;
class SkBitmap;

// This is the browser-side, per-tab implementation of the embeddedSearch API
// (see https://www.chromium.org/embeddedsearch).
class SearchTabHelper : public content::WebContentsObserver,
                        public content::WebContentsUserData<SearchTabHelper>,
                        public InstantServiceObserver,
                        public SearchIPCRouter::Delegate,
                        public OmniboxTabHelper::Observer {
 public:
  SearchTabHelper(const SearchTabHelper&) = delete;
  SearchTabHelper& operator=(const SearchTabHelper&) = delete;

  ~SearchTabHelper() override;

  static void BindEmbeddedSearchConnecter(
      mojo::PendingAssociatedReceiver<search::mojom::EmbeddedSearchConnector>
          receiver,
      content::RenderFrameHost* rfh);

  // Called when the tab corresponding to |this| instance is activated.
  void OnTabActivated();

  // Called when the tab corresponding to |this| instance is deactivated.
  void OnTabDeactivated();

  SearchIPCRouter& ipc_router_for_testing() { return ipc_router_; }

 private:
  friend class content::WebContentsUserData<SearchTabHelper>;
  friend class SearchIPCRouterTest;

  explicit SearchTabHelper(content::WebContents* web_contents);

  // Overridden from contents::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
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

  // Overridden from InstantServiceObserver:
  void NtpThemeChanged(NtpTheme theme) override;
  void MostVisitedInfoChanged(
      const InstantMostVisitedInfo& most_visited_info) override;

  // Overridden from OmniboxTabHelper::Observer:
  void OnOmniboxInputStateChanged() override;
  void OnOmniboxInputInProgress(bool in_progress) override {}
  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override;
  void OnOmniboxPopupVisibilityChanged(bool popup_is_open) override {}

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

  void CloseNTPCustomizeChromeFeaturePromo();

  SearchIPCRouter ipc_router_;

  raw_ptr<InstantService> instant_service_;

  bool is_setting_title_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<SearchTabHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
