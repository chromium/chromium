// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_TAB_CONTENTS_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_TAB_CONTENTS_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace network {
class SimpleURLLoader;
}  // namespace network

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
        const content::NativeWebKeyboardEvent& event) = 0;

    virtual content::WebContents* OpenURLFromTab(
        content::WebContents* source,
        const content::OpenURLParams& params) = 0;

    // Notifies the delegate that the side panel's availability has changed.
    // This is called in response to validating that the side panel SRP is
    // available in `TestSRPAvailability()`. `should_close` determines whether
    // the side panel should be closed. This allows the helper to signal
    // delegates that they should close the feature when something exceptional
    // has happened.
    virtual void SidePanelAvailabilityChanged(bool should_close) = 0;

    virtual void OpenSidePanel() = 0;
  };

  ~SideSearchTabContentsHelper() override;

  // SideContentsWrapper::Delegate:
  void NavigateInTabContents(const content::OpenURLParams& params) override;
  void LastSearchURLUpdated(const GURL& url) override;
  void SidePanelProcessGone() override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // SideSearchConfig::Observer:
  void OnSideSearchConfigChanged() override;

  // Gets the `side_panel_contents_` for the tab. Creates one if it does not
  // currently exist.
  content::WebContents* GetSidePanelContents();

  // Called by clients as a hint to the tab helper to clear away its
  // `side_panel_contents_` if it exists. Caching strategies can leverage this
  // hint and reset the `side_panel_contents_` at some later point in time.
  void ClearSidePanelContents();

  // Returns true if the side panel can be shown for the currently committed
  // navigation entry.
  bool CanShowSidePanelForCommittedNavigation();

  void SetDelegate(base::WeakPtr<Delegate> delegate);

  bool returned_to_previous_srp() const { return returned_to_previous_srp_; }

  bool toggled_open() const { return toggled_open_; }
  void set_toggled_open(bool toggled_open) { toggled_open_ = toggled_open; }

  void SetSidePanelContentsForTesting(
      std::unique_ptr<content::WebContents> side_panel_contents);

  content::WebContents* side_panel_contents_for_testing() const {
    return side_panel_contents_.get();
  }

  const absl::optional<GURL>& last_search_url() { return last_search_url_; }

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

  // Makes a HEAD request for the side search Google SRP to test for the page's
  // availability and sets `is_side_panel_srp_available_` accordingly.
  void TestSRPAvailability();
  void OnResponseLoaded(scoped_refptr<net::HttpResponseHeaders> headers);

  SideSearchConfig* GetConfig();

  // Use a weak ptr for the delegate to avoid issues whereby the tab contents
  // could outlive the delegate.
  base::WeakPtr<Delegate> delegate_;

  // The last Google search URL encountered by this tab contents.
  absl::optional<GURL> last_search_url_;

  // Whether the last search url was the result of the user navigating back
  // to the previously visisted search url. Used to detect cases where the
  // side search panel would be of use to the user and thus could benefit
  // of IPH promo.
  bool returned_to_previous_srp_ = false;

  // A flag to track whether the current tab has its side panel toggled open.
  // Only used with the kSideSearchStatePerTab flag.
  bool toggled_open_ = false;

  // The side panel contents associated with this tab contents.
  // TODO(tluk): Update the way we manage the `side_panel_contents_` to avoid
  // keeping the object around when not needed by the feature.
  std::unique_ptr<content::WebContents> side_panel_contents_;

  // Used to test if the side panel SRP for `last_search_url_` is currently
  // available. Reset every time `TestSRPAvailability()` is called.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  base::ScopedObservation<SideSearchConfig, SideSearchConfig::Observer>
      config_observation_{this};

  base::WeakPtrFactory<SideSearchTabContentsHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_TAB_CONTENTS_HELPER_H_
