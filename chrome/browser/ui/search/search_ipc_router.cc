// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "components/search/search.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
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

}  // namespace

class EmbeddedSearchClientFactoryImpl
    : public SearchIPCRouter::EmbeddedSearchClientFactory,
      public search::mojom::EmbeddedSearchConnector {
 public:
  // |web_contents| and |binding| must outlive this object.
  EmbeddedSearchClientFactoryImpl(
      content::WebContents* web_contents,
      mojo::AssociatedReceiver<search::mojom::EmbeddedSearch>* receiver)
      : client_receiver_(receiver), factory_receivers_(web_contents, this) {
    DCHECK(web_contents);
    DCHECK(receiver);
    // Before we are connected to a frame we throw away all messages.
    embedded_search_client_.reset();
  }

  EmbeddedSearchClientFactoryImpl(const EmbeddedSearchClientFactoryImpl&) =
      delete;
  EmbeddedSearchClientFactoryImpl& operator=(
      const EmbeddedSearchClientFactoryImpl&) = delete;

  search::mojom::EmbeddedSearchClient* GetEmbeddedSearchClient() override {
    return embedded_search_client_.is_bound() ? embedded_search_client_.get()
                                              : nullptr;
  }

  void BindFactoryReceiver(mojo::PendingAssociatedReceiver<
                               search::mojom::EmbeddedSearchConnector> receiver,
                           content::RenderFrameHost* rfh) override {
    factory_receivers_.Bind(rfh, std::move(receiver));
  }

 private:
  void Connect(
      mojo::PendingAssociatedReceiver<search::mojom::EmbeddedSearch> receiver,
      mojo::PendingAssociatedRemote<search::mojom::EmbeddedSearchClient> client)
      override;

  // An interface used to push updates to the frame that connected to us. Before
  // we've been connected to a frame, messages sent on this interface go into
  // the void.
  mojo::AssociatedRemote<search::mojom::EmbeddedSearchClient>
      embedded_search_client_;

  // Used to bind incoming pending receivers to the implementation, which lives
  // in SearchIPCRouter.
  raw_ptr<mojo::AssociatedReceiver<search::mojom::EmbeddedSearch>>
      client_receiver_;

  // Receivers used to listen to connection requests.
  content::RenderFrameHostReceiverSet<search::mojom::EmbeddedSearchConnector>
      factory_receivers_;
};

void EmbeddedSearchClientFactoryImpl::Connect(
    mojo::PendingAssociatedReceiver<search::mojom::EmbeddedSearch> receiver,
    mojo::PendingAssociatedRemote<search::mojom::EmbeddedSearchClient> client) {
  content::RenderFrameHost* frame = factory_receivers_.GetCurrentTargetFrame();
  const bool is_main_frame = frame->GetParent() == nullptr;
  if (!IsInInstantProcess(frame) || !is_main_frame) {
    return;
  }
  client_receiver_->reset();
  client_receiver_->Bind(std::move(receiver));
  embedded_search_client_.reset();
  embedded_search_client_.Bind(std::move(client));
}

SearchIPCRouter::SearchIPCRouter(content::WebContents* web_contents,
                                 Delegate* delegate,
                                 std::unique_ptr<Policy> policy)
    : delegate_(delegate),
      policy_(std::move(policy)),
      commit_counter_(0),
      is_active_tab_(false),
      embedded_search_client_factory_(
          new EmbeddedSearchClientFactoryImpl(web_contents, &receiver_)) {
  DCHECK(delegate);
  DCHECK(policy_.get());
}

SearchIPCRouter::~SearchIPCRouter() = default;

void SearchIPCRouter::BindEmbeddedSearchConnecter(
    mojo::PendingAssociatedReceiver<search::mojom::EmbeddedSearchConnector>
        receiver,
    content::RenderFrameHost* rfh) {
  embedded_search_client_factory_->BindFactoryReceiver(std::move(receiver),
                                                       rfh);
}

void SearchIPCRouter::OnNavigationEntryCommitted() {
  ++commit_counter_;
  if (!embedded_search_client())
    return;
  embedded_search_client()->SetPageSequenceNumber(commit_counter_);
}

void SearchIPCRouter::SetInputInProgress(bool input_in_progress) {
  if (!policy_->ShouldSendSetInputInProgress(is_active_tab_) ||
      !embedded_search_client()) {
    return;
  }

  embedded_search_client()->SetInputInProgress(input_in_progress);
}

void SearchIPCRouter::OmniboxFocusChanged(OmniboxFocusState state,
                                          OmniboxFocusChangeReason reason) {
  if (!policy_->ShouldSendOmniboxFocusChanged() || !embedded_search_client())
    return;

  embedded_search_client()->FocusChanged(state, reason);
}

void SearchIPCRouter::SendMostVisitedInfo(
    const InstantMostVisitedInfo& most_visited_info) {
  if (!policy_->ShouldSendMostVisitedInfo() || !embedded_search_client())
    return;

  embedded_search_client()->MostVisitedInfoChanged(most_visited_info);
}

void SearchIPCRouter::SendNtpTheme(const NtpTheme& theme) {
  if (!policy_->ShouldSendNtpTheme() || !embedded_search_client())
    return;

  embedded_search_client()->ThemeChanged(theme);
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

void SearchIPCRouter::set_delegate_for_testing(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void SearchIPCRouter::set_policy_for_testing(std::unique_ptr<Policy> policy) {
  DCHECK(policy);
  policy_ = std::move(policy);
}
