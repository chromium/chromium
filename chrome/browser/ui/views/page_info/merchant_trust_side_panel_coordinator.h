// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_MERCHANT_TRUST_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_MERCHANT_TRUST_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/page_info/core/page_info_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class BrowserView;
class WebViewSidePanelView;
class SidePanelEntryScope;
class SidePanelUI;

namespace views {
class View;
}  // namespace views

// MerchantTrustSidePanelCoordinator handles the creation and registration of
// the WebViewSidePanelView.
class MerchantTrustSidePanelCoordinator
    : public content::WebContentsUserData<MerchantTrustSidePanelCoordinator>,
      public content::WebContentsObserver {
 public:
  explicit MerchantTrustSidePanelCoordinator(
      content::WebContents* web_contents);
  MerchantTrustSidePanelCoordinator(const MerchantTrustSidePanelCoordinator&) =
      delete;
  MerchantTrustSidePanelCoordinator& operator=(
      const MerchantTrustSidePanelCoordinator&) = delete;
  ~MerchantTrustSidePanelCoordinator() override;

  // Registers MerchantTrust entry in the side panel but does not show it.
  void RegisterEntry(const GURL& merchant_reviews_url);

  // Registers MerchantTrust entry in the side panel and shows side panel with
  // the entry selected if its not shown.
  void RegisterEntryAndShow(const GURL& merchant_reviews_url);

 private:
  friend class content::WebContentsUserData<MerchantTrustSidePanelCoordinator>;

  BrowserView* GetBrowserView() const;

  SidePanelUI* GetSidePanelUI();

  // Called when SidePanel is opened.
  std::unique_ptr<views::View> CreateMerchantTrustWebView(
      SidePanelEntryScope& scope);

  // Called to get the URL for the "open in new tab" button.
  GURL GetOpenInNewTabUrl();

  // content::WebContentsObserver:
  // Override DidFinishNavigation to ensure that the MerchantTrust side panel
  // is closed or updates when the user navigates to a different site and
  // that cached data is cleaned up.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  Profile* GetProfile() const;

  // Stores the |url_params| for the MerchantTrust SidePanel and the
  // |context_url| that they are associated with.
  struct URLInfo {
    // URL of the page this side panel is related to.
    GURL context_url;
    // URL of the side panel button for opening its content in a new tab.
    GURL new_tab_url;
    // Parameters for opening the side panel.
    content::OpenURLParams url_params;
  };

  // Stores the OpenURLParams that were last registered and the URL of the
  // site that these params belong to.
  std::optional<URLInfo> last_url_info_;

  // Stores whether a SidePanel entry has been shown yet or is just registered
  // at pageload. Used to differentiate SidePanels previously opened or opened
  // from PageInfo from panels opened directly through the SidePanel dropdown.
  bool registered_but_not_shown_ = false;

  base::WeakPtr<WebViewSidePanelView> web_view_side_panel_view_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  void GetMerchantTrustInfo(const GURL& url,
                            page_info::MerchantDataCallback callback) const;

  void OnMerchantTrustDataFetched(
      const GURL& url,
      std::optional<page_info::MerchantData> merchant_data);

  base::WeakPtrFactory<MerchantTrustSidePanelCoordinator> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_MERCHANT_TRUST_SIDE_PANEL_COORDINATOR_H_
