// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "components/search/search.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
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
  mojo::AssociatedReceiver<search::mojom::EmbeddedSearch>* client_receiver_;

  // Receivers used to listen to connection requests.
  content::RenderFrameHostReceiverSet<search::mojom::EmbeddedSearchConnector>
      factory_receivers_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedSearchClientFactoryImpl);
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
    : WebContentsObserver(web_contents),
      delegate_(delegate),
      policy_(std::move(policy)),
      commit_counter_(0),
      is_active_tab_(false),
      embedded_search_client_factory_(
          new EmbeddedSearchClientFactoryImpl(web_contents, &receiver_)) {
  DCHECK(web_contents);
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

void SearchIPCRouter::SendLocalBackgroundSelected() {
  if (!policy_->ShouldSendLocalBackgroundSelected() ||
      !embedded_search_client()) {
    return;
  }

  embedded_search_client()->LocalBackgroundSelected();
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

void SearchIPCRouter::LogEvent(int page_seq_no,
                               NTPLoggingEventType event,
                               base::TimeDelta time) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogEvent(event, time);
}

void SearchIPCRouter::LogSuggestionEventWithValue(
    int page_seq_no,
    NTPSuggestionsLoggingEventType event,
    int data,
    base::TimeDelta time) {
  if (page_seq_no != commit_counter_)
    return;

  if (!policy_->ShouldProcessLogSuggestionEventWithValue())
    return;

  delegate_->OnLogSuggestionEventWithValue(event, data, time);
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

void SearchIPCRouter::SetCustomBackgroundInfo(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const std::string& collection_id) {
  if (!policy_->ShouldProcessSetCustomBackgroundInfo())
    return;

  delegate_->OnSetCustomBackgroundInfo(background_url, attribution_line_1,
                                       attribution_line_2, action_url,
                                       collection_id);
}

void SearchIPCRouter::SelectLocalBackgroundImage() {
  if (!policy_->ShouldProcessSelectLocalBackgroundImage())
    return;

  delegate_->OnSelectLocalBackgroundImage();
}

void SearchIPCRouter::BlocklistSearchSuggestion(int32_t task_version,
                                                int64_t task_id) {
  if (!policy_->ShouldProcessBlocklistSearchSuggestion())
    return;

  delegate_->OnBlocklistSearchSuggestion(task_version, task_id);
}

void SearchIPCRouter::BlocklistSearchSuggestionWithHash(
    int32_t task_version,
    int64_t task_id,
    const std::vector<uint8_t>& hash) {
  if (!policy_->ShouldProcessBlocklistSearchSuggestionWithHash())
    return;

  if (hash.size() > 4) {
    return;
  }
  delegate_->OnBlocklistSearchSuggestionWithHash(task_version, task_id,
                                                 hash.data());
}

void SearchIPCRouter::SearchSuggestionSelected(
    int32_t task_version,
    int64_t task_id,
    const std::vector<uint8_t>& hash) {
  if (!policy_->ShouldProcessSearchSuggestionSelected())
    return;

  if (hash.size() > 4) {
    return;
  }
  delegate_->OnSearchSuggestionSelected(task_version, task_id, hash.data());
}

void SearchIPCRouter::OptOutOfSearchSuggestions() {
  if (!policy_->ShouldProcessOptOutOfSearchSuggestions())
    return;

  delegate_->OnOptOutOfSearchSuggestions();
}

void SearchIPCRouter::ApplyDefaultTheme() {
  if (!policy_->ShouldProcessThemeChangeMessages())
    return;

  delegate_->OnApplyDefaultTheme();
}

void SearchIPCRouter::ApplyAutogeneratedTheme(SkColor color) {
  if (!policy_->ShouldProcessThemeChangeMessages())
    return;

  delegate_->OnApplyAutogeneratedTheme(color);
}

void SearchIPCRouter::RevertThemeChanges() {
  if (!policy_->ShouldProcessThemeChangeMessages())
    return;

  delegate_->OnRevertThemeChanges();
}

void SearchIPCRouter::ConfirmThemeChanges() {
  if (!policy_->ShouldProcessThemeChangeMessages())
    return;

  delegate_->OnConfirmThemeChanges();
}

void SearchIPCRouter::set_delegate_for_testing(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void SearchIPCRouter::set_policy_for_testing(std::unique_ptr<Policy> policy) {
  DCHECK(policy);
  policy_ = std::move(policy);
}
