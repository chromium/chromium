// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_action_client_delegator.h"

OmniboxActionClientDelegator::OmniboxActionClientDelegator(
    OmniboxAction::Client& delegate)
    : delegate_(&delegate) {}

OmniboxActionClientDelegator::~OmniboxActionClientDelegator() = default;

void OmniboxActionClientDelegator::OpenSharingHub() {
  delegate_->OpenSharingHub();
}

void OmniboxActionClientDelegator::NewIncognitoWindow() {
  delegate_->NewIncognitoWindow();
}

void OmniboxActionClientDelegator::OpenIncognitoClearBrowsingDataDialog() {
  delegate_->OpenIncognitoClearBrowsingDataDialog();
}

void OmniboxActionClientDelegator::CloseIncognitoWindows() {
  delegate_->CloseIncognitoWindows();
}

void OmniboxActionClientDelegator::PromptPageTranslation() {
  delegate_->PromptPageTranslation();
}

bool OmniboxActionClientDelegator::OpenJourneys(const std::string& query) {
  return delegate_->OpenJourneys(query);
}

void OmniboxActionClientDelegator::OpenLensOverlay(
    bool show,
    lens::LensOverlayInvocationSource invocation_source) {
  delegate_->OpenLensOverlay(show, invocation_source);
}

bool OmniboxActionClientDelegator::ShouldOpenCoBrowsePanel() const {
  return delegate_->ShouldOpenCoBrowsePanel();
}

void OmniboxActionClientDelegator::OpenCoBrowsePanel() {
  delegate_->OpenCoBrowsePanel();
}

void OmniboxActionClientDelegator::IssueContextualSearchRequest(
    const GURL& destination_url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion) {
  delegate_->IssueContextualSearchRequest(destination_url, match_type,
                                          is_zero_prefix_suggestion);
}

bool OmniboxActionClientDelegator::ShouldOpenComposeboxForAskG() const {
  return delegate_->ShouldOpenComposeboxForAskG();
}

void OmniboxActionClientDelegator::OpenComposeboxForAskG() {
  delegate_->OpenComposeboxForAskG();
}
