// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/common/search/search.mojom.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

#if BUILDFLAG(IS_ANDROID)
#error "Instant is only used on desktop";
#endif

class GURL;

namespace content {
class RenderFrameHost;
class WebContents;
}

class SearchIPCRouterTest;

// SearchIPCRouter is responsible for receiving and sending IPC messages between
// the browser and the Instant page.
class SearchIPCRouter : public search::mojom::EmbeddedSearch {
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
    virtual bool ShouldSendSetInputInProgress(bool is_active_tab) = 0;
    virtual bool ShouldSendOmniboxFocusChanged() = 0;
    virtual bool ShouldSendMostVisitedInfo() = 0;
    virtual bool ShouldSendNtpTheme() = 0;
    virtual bool ShouldProcessThemeChangeMessages() = 0;
  };

  // Creates search::mojom::EmbeddedSearchClient connections on request.
  class EmbeddedSearchClientFactory {
   public:
    EmbeddedSearchClientFactory() = default;

    EmbeddedSearchClientFactory(const EmbeddedSearchClientFactory&) = delete;
    EmbeddedSearchClientFactory& operator=(const EmbeddedSearchClientFactory&) =
        delete;

    virtual ~EmbeddedSearchClientFactory() = default;

    // The returned pointer is owned by the factory.
    virtual search::mojom::EmbeddedSearchClient* GetEmbeddedSearchClient() = 0;

    virtual void BindFactoryReceiver(
        mojo::PendingAssociatedReceiver<search::mojom::EmbeddedSearchConnector>
            receiver,
        content::RenderFrameHost* rfh) = 0;
  };

  SearchIPCRouter(content::WebContents* web_contents,
                  Delegate* delegate,
                  std::unique_ptr<Policy> policy);

  SearchIPCRouter(const SearchIPCRouter&) = delete;
  SearchIPCRouter& operator=(const SearchIPCRouter&) = delete;

  ~SearchIPCRouter() override;

  void BindEmbeddedSearchConnecter(
      mojo::PendingAssociatedReceiver<search::mojom::EmbeddedSearchConnector>
          receiver,
      content::RenderFrameHost* rfh);

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

  // Called when the tab corresponding to |this| instance is activated.
  void OnTabActivated();

  // Called when the tab corresponding to |this| instance is deactivated.
  void OnTabDeactivated();

  // search::mojom::EmbeddedSearch:
  void FocusOmnibox(int page_id, bool focus) override;
  void DeleteMostVisitedItem(int page_seq_no, const GURL& url) override;
  void UndoMostVisitedDeletion(int page_seq_no, const GURL& url) override;
  void UndoAllMostVisitedDeletions(int page_seq_no) override;
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

  raw_ptr<Delegate> delegate_;
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
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_
