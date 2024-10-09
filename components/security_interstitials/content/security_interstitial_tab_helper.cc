// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/security_interstitial_tab_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/navigation_handle.h"

namespace {

bool IsInPrimaryMainFrameOrSubFrame(
    content::NavigationHandle* navigation_handle) {
  return (navigation_handle->GetNavigatingFrameType() ==
              content::FrameType::kPrimaryMainFrame ||
          (navigation_handle->GetNavigatingFrameType() ==
               content::FrameType::kSubframe &&
           navigation_handle->GetParentFrame()->IsActive()));
}

}  // namespace
namespace security_interstitials {

SecurityInterstitialTabHelper::~SecurityInterstitialTabHelper() = default;

void SecurityInterstitialTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument() ||
      !IsInPrimaryMainFrameOrSubFrame(navigation_handle)) {
    return;
  }

  int64_t navigation_id = navigation_handle->GetNavigationId();
  content::FrameTreeNodeId frame_tree_node_id =
      navigation_handle->GetFrameTreeNodeId();
  if (navigation_handle->HasCommitted()) {
    // Prepare any existing interstitial in this frame for removal.
    if (IsInterstitialCommittedForFrame(frame_tree_node_id)) {
      base::UmaHistogramEnumeration("interstitial.CloseReason",
                                    InterstitialCloseReason::NAVIGATE_AWAY);
      blocking_documents_for_committed_navigations_.find(frame_tree_node_id)
          ->second->OnInterstitialClosing();
    }

    if (IsInterstitialPendingForNavigation(navigation_id)) {
      blocking_documents_for_committed_navigations_.insert_or_assign(
          frame_tree_node_id,
          std::move(
              blocking_documents_for_pending_navigations_[navigation_id]));
      // The navigation has been committed, update interstitial state to
      // reflect the interstitial is being shown.
      base::UmaHistogramEnumeration(
          "interstitial.CloseReason",
          InterstitialCloseReason::INTERSTITIAL_SHOWN);
      blocking_documents_for_committed_navigations_.find(frame_tree_node_id)
          ->second->OnInterstitialShown();
      blocking_documents_for_pending_navigations_.erase(navigation_id);
    } else {
      // There is no new interstitial for navigation_id, so clear any previously
      // committed blocking document in the same frame.
      blocking_documents_for_committed_navigations_.erase(frame_tree_node_id);
    }
  } else if (IsInterstitialPendingForNavigation(navigation_id)) {
    // Stop tracking the associated navigation since it has not committed.
    blocking_documents_for_pending_navigations_.erase(navigation_id);
  }

  // Interstitials may change the visibility of the URL or other security state.
  web_contents()->DidChangeVisibleSecurityState();
}

void SecurityInterstitialTabHelper::WebContentsDestroyed() {
  content::FrameTreeNodeId main_frame_tree_node_id =
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  // Record a tab closing event if the main frame is displaying an interstitial.
  if (IsInterstitialCommittedForFrame(main_frame_tree_node_id)) {
    base::UmaHistogramEnumeration("interstitial.CloseReason",
                                  InterstitialCloseReason::CLOSE_TAB);
    blocking_documents_for_committed_navigations_.find(main_frame_tree_node_id)
        ->second->OnInterstitialClosing();
  }
  blocking_documents_for_committed_navigations_.clear();
}

void SecurityInterstitialTabHelper::FrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  content::FrameTreeNodeId main_frame_tree_node_id =
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  // Primary main frame interstitial closing is processed on
  // WebContentsDestroyed.
  // TODO(crbug.com/370683964): Investigate assumption in safe browsing that
  // requires main frame interstitials to be cleared in WebContentsDestroyed.
  if (!IsInterstitialCommittedForFrame(frame_tree_node_id) ||
      main_frame_tree_node_id == frame_tree_node_id) {
    return;
  }
  blocking_documents_for_committed_navigations_.find(frame_tree_node_id)
      ->second->OnInterstitialClosing();
  blocking_documents_for_committed_navigations_.erase(frame_tree_node_id);
}

// static
void SecurityInterstitialTabHelper::AssociateBlockingPage(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        blocking_page) {
  // An interstitial should not be shown in a prerendered page or in a fenced
  // frame. The prerender should just be canceled.
  CHECK(IsInPrimaryMainFrameOrSubFrame(navigation_handle));

  // CreateForWebContents() creates a tab helper if it doesn't yet exist for the
  // WebContents provided by |navigation_handle|.
  auto* web_contents = navigation_handle->GetWebContents();
  SecurityInterstitialTabHelper::CreateForWebContents(web_contents);

  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents);
  helper->SetBlockingPage(navigation_handle->GetNavigationId(),
                          std::move(blocking_page));
}

// static
void SecurityInterstitialTabHelper::BindInterstitialCommands(
    mojo::PendingAssociatedReceiver<
        security_interstitials::mojom::InterstitialCommands> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* tab_helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->receivers_.Bind(rfh, std::move(receiver));
}

bool SecurityInterstitialTabHelper::IsInterstitialPendingForNavigation(
    int64_t navigation_id) const {
  return base::Contains(blocking_documents_for_pending_navigations_,
                        navigation_id);
}

bool SecurityInterstitialTabHelper::ShouldDisplayURL() const {
  SecurityInterstitialPage* blocking_page = GetBlockingPageForMainFrame();
  // If the main frame does not display a blocking interstitial then default to
  // being able to display the URL.
  return blocking_page ? blocking_page->ShouldDisplayURL() : true;
}

bool SecurityInterstitialTabHelper::IsDisplayingInterstitial() const {
  return !blocking_documents_for_committed_navigations_.empty();
}

bool SecurityInterstitialTabHelper::HasPendingOrActiveInterstitial() const {
  return !blocking_documents_for_pending_navigations_.empty() ||
         IsDisplayingInterstitial();
}

bool SecurityInterstitialTabHelper::IsInterstitialCommittedForFrame(
    content::FrameTreeNodeId frame_tree_node_id) const {
  return base::Contains(blocking_documents_for_committed_navigations_,
                        frame_tree_node_id);
}

security_interstitials::SecurityInterstitialPage*
SecurityInterstitialTabHelper::
    GetBlockingPageForCurrentlyCommittedNavigationForTesting() {
  // TODO(crbug.com/369759355): Support retrieving blocking page by frame ID.
  content::FrameTreeNodeId frame_tree_node_id =
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  if (IsInterstitialCommittedForFrame(frame_tree_node_id)) {
    return blocking_documents_for_committed_navigations_
        .find(frame_tree_node_id)
        ->second.get();
  }
  return nullptr;
}

SecurityInterstitialTabHelper::SecurityInterstitialTabHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<SecurityInterstitialTabHelper>(
          *web_contents),
      receivers_(web_contents, this) {}

void SecurityInterstitialTabHelper::SetBlockingPage(
    int64_t navigation_id,
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        blocking_page) {
  blocking_documents_for_pending_navigations_[navigation_id] =
      std::move(blocking_page);
}

SecurityInterstitialPage*
SecurityInterstitialTabHelper::GetBlockingPageForCurrentTargetFrame() {
  CHECK(!blocking_documents_for_committed_navigations_.empty());
  auto* render_frame_host = receivers_.GetCurrentTargetFrame();
  content::FrameTreeNodeId id = render_frame_host->GetFrameTreeNodeId();
  CHECK(IsInterstitialCommittedForFrame(id));
  return blocking_documents_for_committed_navigations_.find(id)->second.get();
}

SecurityInterstitialPage*
SecurityInterstitialTabHelper::GetBlockingPageForMainFrame() const {
  content::FrameTreeNodeId frame_tree_node_id =
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  if (IsInterstitialCommittedForFrame(frame_tree_node_id)) {
    return blocking_documents_for_committed_navigations_
        .find(frame_tree_node_id)
        ->second.get();
  }
  return nullptr;
}

void SecurityInterstitialTabHelper::HandleCommand(
    security_interstitials::SecurityInterstitialCommand cmd) {
  // TODO(crbug.com/369784619): Currently commands need to be converted to
  // strings before passing them to CommandReceived, which then turns them into
  // integers again, this redundant conversion should be removed.
  //
  // HandleCommand is only called in response to a Mojo message sent from frames
  // that have a committed interstitial. This ensures that the current target
  // frame is present and can process the corresponding command received.
  GetBlockingPageForCurrentTargetFrame()->CommandReceived(
      base::NumberToString(cmd));
}

void SecurityInterstitialTabHelper::DontProceed() {
  HandleCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_DONT_PROCEED);
}

void SecurityInterstitialTabHelper::Proceed() {
  HandleCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_PROCEED);
}

void SecurityInterstitialTabHelper::ShowMoreSection() {
  HandleCommand(security_interstitials::SecurityInterstitialCommand::
                    CMD_SHOW_MORE_SECTION);
}

void SecurityInterstitialTabHelper::OpenHelpCenter() {
  HandleCommand(security_interstitials::SecurityInterstitialCommand::
                    CMD_OPEN_HELP_CENTER);
}

void SecurityInterstitialTabHelper::OpenDiagnostic() {
  HandleCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_OPEN_DIAGNOSTIC);
}

void SecurityInterstitialTabHelper::Reload() {
  HandleCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_RELOAD);
}

void SecurityInterstitialTabHelper::OpenDateSettings() {
  HandleCommand(security_interstitials::SecurityInterstitialCommand::
                    CMD_OPEN_DATE_SETTINGS);
}

void SecurityInterstitialTabHelper::OpenLogin() {
  HandleCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_OPEN_LOGIN);
}

void SecurityInterstitialTabHelper::DoReport() {
  HandleCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_DO_REPORT);
}

void SecurityInterstitialTabHelper::DontReport() {
  HandleCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_DONT_REPORT);
}

void SecurityInterstitialTabHelper::OpenReportingPrivacy() {
  HandleCommand(security_interstitials::SecurityInterstitialCommand::
                    CMD_OPEN_REPORTING_PRIVACY);
}

void SecurityInterstitialTabHelper::OpenWhitepaper() {
  HandleCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_OPEN_WHITEPAPER);
}

void SecurityInterstitialTabHelper::ReportPhishingError() {
  HandleCommand(security_interstitials::SecurityInterstitialCommand::
                    CMD_REPORT_PHISHING_ERROR);
}

void SecurityInterstitialTabHelper::OpenEnhancedProtectionSettings() {
  HandleCommand(security_interstitials::SecurityInterstitialCommand::
                    CMD_OPEN_ENHANCED_PROTECTION_SETTINGS);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SecurityInterstitialTabHelper);

}  //  namespace security_interstitials
