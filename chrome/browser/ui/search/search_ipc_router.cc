// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/render_messages.h"
#include "components/search/search.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace {

bool IsInInstantProcess(content::RenderFrameHost* render_frame) {
  content::RenderProcessHost* process_host = render_frame->GetProcess();
  const InstantService* instant_service = InstantServiceFactory::GetForProfile(
      Profile::FromBrowserContext(process_host->GetBrowserContext()));
  if (!instant_service)
    return false;

  return instant_service->IsInstantProcess(process_host->GetID());
}

class EmbeddedSearchClientFactoryImpl
    : public SearchIPCRouter::EmbeddedSearchClientFactory,
      public chrome::mojom::EmbeddedSearchConnector {
 public:
  // |web_contents| and |binding| must outlive this object.
  EmbeddedSearchClientFactoryImpl(
      content::WebContents* web_contents,
      mojo::AssociatedBinding<chrome::mojom::EmbeddedSearch>* binding)
      : client_binding_(binding), factory_bindings_(web_contents, this) {
    DCHECK(web_contents);
    DCHECK(binding);
    // Before we are connected to a frame we throw away all messages.
    mojo::MakeRequestAssociatedWithDedicatedPipe(&embedded_search_client_);
  }

  chrome::mojom::EmbeddedSearchClient* GetEmbeddedSearchClient() override {
    return embedded_search_client_.get();
  }

 private:
  void Connect(
      chrome::mojom::EmbeddedSearchAssociatedRequest request,
      chrome::mojom::EmbeddedSearchClientAssociatedPtrInfo client) override;

  // An interface used to push updates to the frame that connected to us. Before
  // we've been connected to a frame, messages sent on this interface go into
  // the void.
  chrome::mojom::EmbeddedSearchClientAssociatedPtr embedded_search_client_;

  // Used to bind incoming interface requests to the implementation, which lives
  // in SearchIPCRouter.
  mojo::AssociatedBinding<chrome::mojom::EmbeddedSearch>* client_binding_;

  // Binding used to listen to connection requests.
  content::WebContentsFrameBindingSet<chrome::mojom::EmbeddedSearchConnector>
      factory_bindings_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedSearchClientFactoryImpl);
};

void EmbeddedSearchClientFactoryImpl::Connect(
    chrome::mojom::EmbeddedSearchAssociatedRequest request,
    chrome::mojom::EmbeddedSearchClientAssociatedPtrInfo client) {
  content::RenderFrameHost* frame = factory_bindings_.GetCurrentTargetFrame();
  const bool is_main_frame = frame->GetParent() == nullptr;
  if (!IsInInstantProcess(frame) || !is_main_frame) {
    return;
  }
  client_binding_->Bind(std::move(request));
  embedded_search_client_.Bind(std::move(client));
}

}  // namespace

SearchIPCRouter::SearchIPCRouter(content::WebContents* web_contents,
                                 Delegate* delegate,
                                 std::unique_ptr<Policy> policy)
    : WebContentsObserver(web_contents),
      delegate_(delegate),
      policy_(std::move(policy)),
      commit_counter_(0),
      is_active_tab_(false),
      binding_(this),
      embedded_search_client_factory_(
          new EmbeddedSearchClientFactoryImpl(web_contents, &binding_)) {
  DCHECK(web_contents);
  DCHECK(delegate);
  DCHECK(policy_.get());
}

SearchIPCRouter::~SearchIPCRouter() = default;

void SearchIPCRouter::OnNavigationEntryCommitted() {
  ++commit_counter_;
  embedded_search_client()->SetPageSequenceNumber(commit_counter_);
}

void SearchIPCRouter::SetInputInProgress(bool input_in_progress) {
  if (!policy_->ShouldSendSetInputInProgress(is_active_tab_))
    return;

  embedded_search_client()->SetInputInProgress(input_in_progress);
}

void SearchIPCRouter::OmniboxFocusChanged(OmniboxFocusState state,
                                          OmniboxFocusChangeReason reason) {
  if (!policy_->ShouldSendOmniboxFocusChanged())
    return;

  embedded_search_client()->FocusChanged(state, reason);
}

void SearchIPCRouter::SendMostVisitedItems(
    const std::vector<InstantMostVisitedItem>& items,
    bool is_custom_links) {
  if (!policy_->ShouldSendMostVisitedItems())
    return;

  embedded_search_client()->MostVisitedChanged(items, is_custom_links);
}

void SearchIPCRouter::SendThemeBackgroundInfo(
    const ThemeBackgroundInfo& theme_info) {
  if (!policy_->ShouldSendThemeBackgroundInfo())
    return;

  embedded_search_client()->ThemeChanged(theme_info);
}

void SearchIPCRouter::OnTabActivated() {
  is_active_tab_ = true;
}

void SearchIPCRouter::OnTabDeactivated() {
  is_active_tab_ = false;
}

void SearchIPCRouter::FocusOmnibox(int page_seq_no, bool focus) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessFocusOmnibox(is_active_tab_))
    return;

  delegate_->FocusOmnibox(focus);
}

void SearchIPCRouter::DeleteMostVisitedItem(int page_seq_no, const GURL& url) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessDeleteMostVisitedItem())
    return;

  delegate_->OnDeleteMostVisitedItem(url);
}

void SearchIPCRouter::UndoMostVisitedDeletion(int page_seq_no,
                                              const GURL& url) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessUndoMostVisitedDeletion())
    return;

  delegate_->OnUndoMostVisitedDeletion(url);
}

void SearchIPCRouter::UndoAllMostVisitedDeletions(int page_seq_no) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessUndoAllMostVisitedDeletions())
    return;

  delegate_->OnUndoAllMostVisitedDeletions();
}

void SearchIPCRouter::AddCustomLink(int page_seq_no,
                                    const GURL& url,
                                    const std::string& title,
                                    AddCustomLinkCallback callback) {
  bool result = false;
  if (page_seq_no == commit_counter_ && policy_->ShouldProcessAddCustomLink()) {
    result = delegate_->OnAddCustomLink(url, title);
  }

  std::move(callback).Run(result);
}

void SearchIPCRouter::UpdateCustomLink(int page_seq_no,
                                       const GURL& url,
                                       const GURL& new_url,
                                       const std::string& new_title,
                                       UpdateCustomLinkCallback callback) {
  bool result = false;
  if (page_seq_no == commit_counter_ &&
      policy_->ShouldProcessUpdateCustomLink()) {
    result = delegate_->OnUpdateCustomLink(url, new_url, new_title);
  }

  std::move(callback).Run(result);
}

void SearchIPCRouter::DeleteCustomLink(int page_seq_no,
                                       const GURL& url,
                                       DeleteCustomLinkCallback callback) {
  bool result = false;
  if (page_seq_no == commit_counter_ &&
      policy_->ShouldProcessDeleteCustomLink()) {
    result = delegate_->OnDeleteCustomLink(url);
  }

  std::move(callback).Run(result);
}

void SearchIPCRouter::UndoCustomLinkAction(int page_seq_no) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessUndoCustomLinkAction())
    return;

  delegate_->OnUndoCustomLinkAction();
}

void SearchIPCRouter::ResetCustomLinks(int page_seq_no) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessResetCustomLinks())
    return;

  delegate_->OnResetCustomLinks();
}

void SearchIPCRouter::DoesUrlResolve(int page_seq_no,
                                     const GURL& url,
                                     DoesUrlResolveCallback callback) {
  if (page_seq_no == commit_counter_ &&
      policy_->ShouldProcessDoesUrlResolve()) {
    delegate_->OnDoesUrlResolve(url, std::move(callback));
  } else {
    std::move(callback).Run(/*resolves=*/true, /*timeout=*/false);
  }
}

void SearchIPCRouter::LogEvent(int page_seq_no,
                               NTPLoggingEventType event,
                               base::TimeDelta time) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogEvent(event, time);
}

void SearchIPCRouter::LogMostVisitedImpression(
    int page_seq_no,
    const ntp_tiles::NTPTileImpression& impression) {
  if (page_seq_no != commit_counter_)
    return;

  // Logging impressions is controlled by the same policy as logging events.
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogMostVisitedImpression(impression);
}

void SearchIPCRouter::LogMostVisitedNavigation(
    int page_seq_no,
    const ntp_tiles::NTPTileImpression& impression) {
  if (page_seq_no != commit_counter_)
    return;

  // Logging navigations is controlled by the same policy as logging events.
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogMostVisitedNavigation(impression);
}

void SearchIPCRouter::PasteAndOpenDropdown(int page_seq_no,
                                           const base::string16& text) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessPasteIntoOmnibox(is_active_tab_))
    return;

  delegate_->PasteIntoOmnibox(text);
}

void SearchIPCRouter::ChromeIdentityCheck(
    int page_seq_no,
    const base::string16& identity,
    ChromeIdentityCheckCallback callback) {
  bool result = false;
  if (page_seq_no == commit_counter_ &&
      policy_->ShouldProcessChromeIdentityCheck()) {
    result = delegate_->ChromeIdentityCheck(identity);
  }

  std::move(callback).Run(result);
}

void SearchIPCRouter::HistorySyncCheck(int page_seq_no,
                                       HistorySyncCheckCallback callback) {
  bool result = false;
  if (page_seq_no == commit_counter_ &&
      policy_->ShouldProcessHistorySyncCheck()) {
    result = delegate_->HistorySyncCheck();
  }

  std::move(callback).Run(result);
}

void SearchIPCRouter::SetCustomBackgroundURL(const GURL& url) {
  if (!policy_->ShouldProcessSetCustomBackgroundURL())
    return;

  delegate_->OnSetCustomBackgroundURL(url);
}

void SearchIPCRouter::SetCustomBackgroundURLWithAttributions(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url) {
  if (!policy_->ShouldProcessSetCustomBackgroundURLWithAttributions())
    return;

  delegate_->OnSetCustomBackgroundURLWithAttributions(
      background_url, attribution_line_1, attribution_line_2, action_url);
}

void SearchIPCRouter::SelectLocalBackgroundImage() {
  if (!policy_->ShouldProcessSelectLocalBackgroundImage())
    return;

  delegate_->OnSelectLocalBackgroundImage();
}

void SearchIPCRouter::set_delegate_for_testing(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void SearchIPCRouter::set_policy_for_testing(std::unique_ptr<Policy> policy) {
  DCHECK(policy);
  policy_ = std::move(policy);
}
