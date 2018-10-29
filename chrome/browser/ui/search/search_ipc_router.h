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
#include "chrome/common/search.mojom.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"

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
                        public chrome::mojom::EmbeddedSearch {
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

    // Called when the EmbeddedSearch wants to delete a custom link.
    virtual bool OnDeleteCustomLink(const GURL& url) = 0;

    // Called when the EmbeddedSearch wants to undo the previous custom link
    // action.
    virtual void OnUndoCustomLinkAction() = 0;

    // Called when the EmbeddedSearch wants to delete all custom links and
    // use Most Visited sites instead.
    virtual void OnResetCustomLinks() = 0;

    // Called when the EmbeddedSearch wants to check if |url| resolves to an
    // existing page.
    virtual void OnDoesUrlResolve(const GURL& url,
                                  DoesUrlResolveCallback callback) = 0;

    // Called to signal that an event has occurred on the New Tab Page at a
    // particular time since navigation start.
    virtual void OnLogEvent(NTPLoggingEventType event,
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

    // Called when the EmbeddedSearch wants to verify the signed-in Chrome
    // identity against the provided |identity|.
    virtual bool ChromeIdentityCheck(const base::string16& identity) = 0;

    // Called when the EmbeddedSearch wants to verify that history sync is
    // enabled.
    virtual bool HistorySyncCheck() = 0;

    // Called when a custom background is selected on the NTP.
    virtual void OnSetCustomBackgroundURL(const GURL& url) = 0;

    // Called when a custom background with attributions is selected on the NTP.
    // background_url: Url of the background image.
    // attribution_line_1: First attribution line for the image.
    // attribution_line_2: Second attribution line for the image.
    // action_url: Url to learn more about the backgrounds image.
    virtual void OnSetCustomBackgroundURLWithAttributions(
        const GURL& background_url,
        const std::string& attribution_line_1,
        const std::string& attribution_line_2,
        const GURL& action_url) = 0;

    // Called to open the file select dialog for selecting a
    // NTP background image.
    virtual void OnSelectLocalBackgroundImage() = 0;
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
    virtual bool ShouldProcessDeleteCustomLink() = 0;
    virtual bool ShouldProcessUndoCustomLinkAction() = 0;
    virtual bool ShouldProcessResetCustomLinks() = 0;
    virtual bool ShouldProcessDoesUrlResolve() = 0;
    virtual bool ShouldProcessLogEvent() = 0;
    virtual bool ShouldProcessPasteIntoOmnibox(bool is_active_tab) = 0;
    virtual bool ShouldProcessChromeIdentityCheck() = 0;
    virtual bool ShouldProcessHistorySyncCheck() = 0;
    virtual bool ShouldSendSetInputInProgress(bool is_active_tab) = 0;
    virtual bool ShouldSendOmniboxFocusChanged() = 0;
    virtual bool ShouldSendMostVisitedItems() = 0;
    virtual bool ShouldSendThemeBackgroundInfo() = 0;
    virtual bool ShouldProcessSetCustomBackgroundURL() = 0;
    virtual bool ShouldProcessSetCustomBackgroundURLWithAttributions() = 0;
    virtual bool ShouldProcessSelectLocalBackgroundImage() = 0;
  };

  // Creates chrome::mojom::EmbeddedSearchClient connections on request.
  class EmbeddedSearchClientFactory {
   public:
    EmbeddedSearchClientFactory() = default;
    virtual ~EmbeddedSearchClientFactory() = default;

    // The returned pointer is owned by the factory.
    virtual chrome::mojom::EmbeddedSearchClient* GetEmbeddedSearchClient() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(EmbeddedSearchClientFactory);
  };

  SearchIPCRouter(content::WebContents* web_contents,
                  Delegate* delegate,
                  std::unique_ptr<Policy> policy);
  ~SearchIPCRouter() override;

  // Tells the SearchIPCRouter that a new page in an Instant process committed.
  void OnNavigationEntryCommitted();

  // Tells the page that user input started or stopped.
  void SetInputInProgress(bool input_in_progress);

  // Tells the page that the omnibox focus has changed.
  void OmniboxFocusChanged(OmniboxFocusState state,
                           OmniboxFocusChangeReason reason);

  // Tells the renderer about the most visited items.
  void SendMostVisitedItems(const std::vector<InstantMostVisitedItem>& items,
                            bool is_custom_links);

  // Tells the renderer about the current theme background.
  void SendThemeBackgroundInfo(const ThemeBackgroundInfo& theme_info);

  // Called when the tab corresponding to |this| instance is activated.
  void OnTabActivated();

  // Called when the tab corresponding to |this| instance is deactivated.
  void OnTabDeactivated();

  // chrome::mojom::EmbeddedSearch:
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
  void DeleteCustomLink(int page_seq_no,
                        const GURL& url,
                        DeleteCustomLinkCallback callback) override;
  void UndoCustomLinkAction(int page_seq_no) override;
  void ResetCustomLinks(int page_seq_no) override;
  void DoesUrlResolve(int page_seq_no,
                      const GURL& url,
                      DoesUrlResolveCallback callback) override;
  void LogEvent(int page_seq_no,
                NTPLoggingEventType event,
                base::TimeDelta time) override;
  void LogMostVisitedImpression(
      int page_seq_no,
      const ntp_tiles::NTPTileImpression& impression) override;
  void LogMostVisitedNavigation(
      int page_seq_no,
      const ntp_tiles::NTPTileImpression& impression) override;
  void PasteAndOpenDropdown(int page_seq_no,
                            const base::string16& text) override;
  void ChromeIdentityCheck(int page_seq_no,
                           const base::string16& identity,
                           ChromeIdentityCheckCallback callback) override;
  void HistorySyncCheck(int page_seq_no,
                        HistorySyncCheckCallback callback) override;
  void SetCustomBackgroundURL(const GURL& url) override;
  void SetCustomBackgroundURLWithAttributions(
      const GURL& background_url,
      const std::string& attribution_line_1,
      const std::string& attribution_line_2,
      const GURL& action_url) override;
  void SelectLocalBackgroundImage() override;
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

  chrome::mojom::EmbeddedSearchClient* embedded_search_client() {
    return embedded_search_client_factory_->GetEmbeddedSearchClient();
  }

  Delegate* delegate_;
  std::unique_ptr<Policy> policy_;

  // Holds the number of main frame commits executed in this tab. Used by the
  // SearchIPCRouter to ensure that delayed IPC replies are ignored.
  int commit_counter_;

  // Set to true, when the tab corresponding to |this| instance is active.
  bool is_active_tab_;

  // Binding for the connected main frame. We only allow one frame to connect at
  // the moment, but this could be extended to a map of connected frames, if
  // desired.
  mojo::AssociatedBinding<chrome::mojom::EmbeddedSearch> binding_;

  std::unique_ptr<EmbeddedSearchClientFactory> embedded_search_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchIPCRouter);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_
