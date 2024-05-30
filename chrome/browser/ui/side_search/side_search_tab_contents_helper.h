// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_TAB_CONTENTS_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_TAB_CONTENTS_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

class GURL;
class SideSearchConfig;

// Side Search helper for the WebContents hosted in the browser's main tab area.
class SideSearchTabContentsHelper
    : public SideSearchSideContentsHelper::Delegate,
      public content::WebContentsObserver,
      public SideSearchConfig::Observer,
      public content::WebContentsUserData<SideSearchTabContentsHelper> {
 public:
  class Delegate {
   public:
    virtual bool HandleKeyboardEvent(
        content::WebContents* source,
        const input::NativeWebKeyboardEvent& event) = 0;

    virtual content::WebContents* OpenURLFromTab(
        content::WebContents* source,
        const content::OpenURLParams& params,
        base::OnceCallback<void(content::NavigationHandle&)>
            navigation_handle_callback) = 0;

    // Notifies the delegate that the side panel's availability has changed.
    // `should_close` determines whether the side panel should be closed. This
    // allows the helper to signal delegates that they should close the feature
    // when something exceptional has happened.
    virtual void SidePanelAvailabilityChanged(bool should_close) = 0;

    virtual void OpenSidePanel() = 0;
  };

  // Holds state reflecting the current current navigation that is the result of
  // a redirect from the side panel to this helper's tab.
  struct SidePanelRedirectInfo {
    GURL initiated_redirect_url;
    bool initiated_via_link;
  };

  ~SideSearchTabContentsHelper() override;

  // SideContentsWrapper::Delegate:
  void NavigateInTabContents(const content::OpenURLParams& params) override;
  void LastSearchURLUpdated(const GURL& url) override;
  void SidePanelProcessGone() override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  content::WebContents* GetTabWebContents() override;
  void CarryOverSideSearchStateToNewTab(
      const GURL& search_url,
      content::WebContents* new_web_contents) override;

  // content::WebContentsObserver:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // SideSearchConfig::Observer:
  void OnSideSearchConfigChanged() override;

  // Gets the `side_panel_contents_` for the tab. Creates one if it does not
  // currently exist.
  content::WebContents* GetSidePanelContents();

  // Flags whether or not the current search journey was automatically triggered
  // (i.e. the user did not explicitly open the side panel).
  void SetAutoTriggered(bool auto_triggered);

  // Called by clients as a hint to the tab helper to clear away its
  // `side_panel_contents_` if it exists. Caching strategies can leverage this
  // hint and reset the `side_panel_contents_` at some later point in time.
  void ClearSidePanelContents();

  // Returns true if the side panel can be shown for the currently committed
  // navigation entry.
  bool CanShowSidePanelForCommittedNavigation();

  // This is called to log the duration between when the side panel was made
  // available to the first time it was opened for the `last_search_url_`. This
  // resets the `available_timer_` to avoid logging multiple times in the case a
  // user closes and opens the panel repeatedly for the same `last_search_url_`.
  void MaybeRecordDurationSidePanelAvailableToFirstOpen();

  void SetDelegate(base::WeakPtr<Delegate> delegate);

  const std::optional<SidePanelRedirectInfo>&
  side_panel_initiated_redirect_info() const {
    return side_panel_initiated_redirect_info_;
  }

  int returned_to_previous_srp_count() const {
    return returned_to_previous_srp_count_;
  }

  bool toggled_open() const { return toggled_open_; }
  void set_toggled_open(bool toggled_open) { toggled_open_ = toggled_open; }

  void SetSidePanelContentsForTesting(
      std::unique_ptr<content::WebContents> side_panel_contents);

  content::WebContents* side_panel_contents_for_testing() const {
    return side_panel_contents_.get();
  }

  const std::optional<GURL>& last_search_url() { return last_search_url_; }

  // Takes the search URL passed from context menu and opens search results in
  // side panel.
  void OpenSidePanelFromContextMenuSearch(const GURL& url);

  // Returns true when the side panel can be actually opened from context menu
  // option.
  bool CanShowSidePanelFromContextMenuSearch();

 private:
  friend class content::WebContentsUserData<SideSearchTabContentsHelper>;
  explicit SideSearchTabContentsHelper(content::WebContents* web_contents);

  // Gets the helper for the side contents.
  SideSearchSideContentsHelper* GetSideContentsHelper();

  // Creates the `side_panel_contents_` associated with this helper's tab
  // contents.
  void CreateSidePanelContents();

  // Navigates `side_panel_contents_` to the tab's `last_search_url_` if needed.
  // Should only be called when `side_contents_active_`.
  void UpdateSideContentsNavigation();

  // Closes the side panel and resets all helper state.
  void ClearHelperState();

  SideSearchConfig* GetConfig();

  // Use a weak ptr for the delegate to avoid issues whereby the tab contents
  // could outlive the delegate.
  base::WeakPtr<Delegate> delegate_;

  // The last Google search URL encountered by this tab contents.
  std::optional<GURL> last_search_url_;

  // Counts the number of times the user has returned to the `last_search_url_`
  // via back navigation. This is used to detect cases where the side search
  // panel would be of use to the user and is used to show an IPH promo and
  // automatically trigger the side panel.
  int returned_to_previous_srp_count_ = 0;

  // A flag to track whether the current tab has its side panel toggled open.
  // Only used with the kSideSearchStatePerTab flag.
  bool toggled_open_ = false;

  // Tracks the URL and initiation state for the side panel initiated redirect.
  // This is used to determine if any redirects that follow belong to the
  // initial redirected request from the side panel. It is not sufficient to
  // rely on NavigationHandles as redirects may be client initiated and new
  // NavigationHandles are created in these cases.
  std::optional<SidePanelRedirectInfo> side_panel_initiated_redirect_info_;

  // The side panel contents associated with this tab contents.
  // TODO(tluk): Update the way we manage the `side_panel_contents_` to avoid
  // keeping the object around when not needed by the feature.
  std::unique_ptr<content::WebContents> side_panel_contents_;

  // Time since the side panel became available for the `last_search_url_`.
  std::optional<base::ElapsedTimer> available_timer_;

  // True if the side panel could be shown for the previously committed
  // navigation.
  bool could_show_for_last_committed_navigation_ = false;

  // Tracks whether the page action icon has animated-in its label text. Track
  // this to ensure we only show the label at most once per tab.
  bool page_action_label_shown_ = false;

  // Tracks the number of times the page action icon has animated-in its label
  // text for this tab.
  int page_action_label_shown_count_ = 0;

  // Tracks if we can show the page action label when the entrypoint is
  // revealed. This is set to true after a navigation to a SRP and reset when
  // the entrypoint for the current SRP is shown. This is done to ensure we only
  // show the label text at most once for a given SRP.
  bool can_show_page_action_label_ = false;

  base::ScopedObservation<SideSearchConfig, SideSearchConfig::Observer>
      config_observation_{this};

  base::WeakPtrFactory<SideSearchTabContentsHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_TAB_CONTENTS_HELPER_H_
