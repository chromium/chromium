// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "base/strings/string_number_conversions.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/navigation_handle.h"

namespace security_interstitials {

SecurityInterstitialTabHelper::~SecurityInterstitialTabHelper() {}

void SecurityInterstitialTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  auto it = blocking_pages_for_navigations_.find(
      navigation_handle->GetNavigationId());

  if (navigation_handle->HasCommitted()) {
    if (blocking_page_for_currently_committed_navigation_) {
      blocking_page_for_currently_committed_navigation_
          ->OnInterstitialClosing();
    }

    if (it == blocking_pages_for_navigations_.end()) {
      blocking_page_for_currently_committed_navigation_.reset();
    } else {
      blocking_page_for_currently_committed_navigation_ = std::move(it->second);
    }
  }

  if (it != blocking_pages_for_navigations_.end()) {
    blocking_pages_for_navigations_.erase(it);
  }

  // Interstitials may change the visibility of the URL or other security state.
  web_contents()->DidChangeVisibleSecurityState();
}

void SecurityInterstitialTabHelper::WebContentsDestroyed() {
  if (blocking_page_for_currently_committed_navigation_) {
    blocking_page_for_currently_committed_navigation_->OnInterstitialClosing();
  }
}

// static
void SecurityInterstitialTabHelper::AssociateBlockingPage(
    content::WebContents* web_contents,
    int64_t navigation_id,
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        blocking_page) {
  // CreateForWebContents() creates a tab helper if it doesn't exist for
  // |web_contents| yet.
  SecurityInterstitialTabHelper::CreateForWebContents(web_contents);

  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents);
  helper->SetBlockingPage(navigation_id, std::move(blocking_page));
}

bool SecurityInterstitialTabHelper::ShouldDisplayURL() const {
  CHECK(IsDisplayingInterstitial());
  return blocking_page_for_currently_committed_navigation_->ShouldDisplayURL();
}

bool SecurityInterstitialTabHelper::IsDisplayingInterstitial() const {
  return blocking_page_for_currently_committed_navigation_ != nullptr;
}

security_interstitials::SecurityInterstitialPage*
SecurityInterstitialTabHelper::
    GetBlockingPageForCurrentlyCommittedNavigationForTesting() {
  return blocking_page_for_currently_committed_navigation_.get();
}

SecurityInterstitialTabHelper::SecurityInterstitialTabHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents), binding_(web_contents, this) {}

void SecurityInterstitialTabHelper::SetBlockingPage(
    int64_t navigation_id,
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        blocking_page) {
  blocking_pages_for_navigations_[navigation_id] = std::move(blocking_page);
}

void SecurityInterstitialTabHelper::HandleCommand(
    security_interstitials::SecurityInterstitialCommand cmd) {
  if (blocking_page_for_currently_committed_navigation_) {
    // Currently commands need to be converted to strings before passing them
    // to CommandReceived, which then turns them into integers again, this
    // redundant conversion will be removed once commited interstitials are the
    // only supported codepath.
    blocking_page_for_currently_committed_navigation_->CommandReceived(
        base::NumberToString(cmd));
  }
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(SecurityInterstitialTabHelper)

}  //  namespace security_interstitials
