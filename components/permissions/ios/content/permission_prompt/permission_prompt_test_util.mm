// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/ios/content/permission_prompt/permission_prompt_test_util.h"

#include "components/permissions/resolvers/permission_prompt_options.h"

namespace permissions {

StubPermissionPromptDelegate::StubPermissionPromptDelegate() = default;
StubPermissionPromptDelegate::~StubPermissionPromptDelegate() = default;

const std::vector<std::unique_ptr<PermissionRequest>>&
StubPermissionPromptDelegate::Requests() {
  return requests_;
}

GURL StubPermissionPromptDelegate::GetRequestingOrigin() const {
  return GURL("https://example.com");
}

GURL StubPermissionPromptDelegate::GetEmbeddingOrigin() const {
  return GURL("https://example.com");
}

void StubPermissionPromptDelegate::Accept(const PromptOptions& prompt_options) {
  accept_called_ = true;
}
void StubPermissionPromptDelegate::AcceptThisTime(
    const PromptOptions& prompt_options) {
  accept_this_time_called_ = true;
}
void StubPermissionPromptDelegate::Deny(const PromptOptions& prompt_options) {
  deny_called_ = true;
}
void StubPermissionPromptDelegate::Dismiss(
    const PromptOptions& prompt_options) {}
void StubPermissionPromptDelegate::Ignore(const PromptOptions& prompt_options) {
}
GeolocationAccuracy
StubPermissionPromptDelegate::GetInitialGeolocationAccuracySelection() const {
  return GeolocationAccuracy::kPrecise;
}
bool StubPermissionPromptDelegate::ShouldShowLocationPrecisionSelector() const {
  return true;
}
void StubPermissionPromptDelegate::FinalizeCurrentRequests() {}
void StubPermissionPromptDelegate::OpenHelpCenterLink(const ui::Event& event) {}
void StubPermissionPromptDelegate::PreIgnoreQuietPrompt() {}

std::optional<PermissionUiSelector::QuietUiReason>
StubPermissionPromptDelegate::ReasonForUsingQuietUi() const {
  return std::nullopt;
}

bool StubPermissionPromptDelegate::ShouldCurrentRequestUseQuietUI() const {
  return false;
}
bool StubPermissionPromptDelegate::ShouldDropCurrentRequestIfCannotShowQuietly()
    const {
  return false;
}
bool StubPermissionPromptDelegate::WasCurrentRequestAlreadyDisplayed() {
  return false;
}

void StubPermissionPromptDelegate::SetDismissOnTabClose() {}
void StubPermissionPromptDelegate::SetPromptShown() {}
void StubPermissionPromptDelegate::SetDecisionTime() {}
void StubPermissionPromptDelegate::SetManageClicked() {}
void StubPermissionPromptDelegate::SetLearnMoreClicked() {}
void StubPermissionPromptDelegate::SetHatsShownCallback(
    base::OnceCallback<void()> callback) {}
void StubPermissionPromptDelegate::SwitchToLoudPrompt() {}

content::WebContents* StubPermissionPromptDelegate::GetAssociatedWebContents() {
  return nullptr;
}

base::WeakPtr<PermissionPrompt::Delegate>
StubPermissionPromptDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool StubPermissionPromptDelegate::RecreateView() {
  return false;
}

const PermissionPrompt* StubPermissionPromptDelegate::GetCurrentPrompt() const {
  return nullptr;
}

void StubPermissionPromptDelegate::AddRequest(
    std::unique_ptr<PermissionRequest> request) {
  requests_.push_back(std::move(request));
}

bool StubPermissionPromptDelegate::accept_called() const {
  return accept_called_;
}
bool StubPermissionPromptDelegate::accept_this_time_called() const {
  return accept_this_time_called_;
}
bool StubPermissionPromptDelegate::deny_called() const {
  return deny_called_;
}

MockPermissionPromptIOS::MockPermissionPromptIOS(
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptIOS(web_contents, delegate) {}

MockPermissionPromptIOS::~MockPermissionPromptIOS() = default;

PermissionPromptDisposition MockPermissionPromptIOS::GetPromptDisposition()
    const {
  return PermissionPromptDisposition::MODAL_DIALOG;
}

NSString* MockPermissionPromptIOS::GetPositiveButtonText(
    bool is_one_time) const {
  return @"";
}

NSString* MockPermissionPromptIOS::GetNegativeButtonText(
    bool is_one_time) const {
  return @"";
}

NSString* MockPermissionPromptIOS::GetPositiveEphemeralButtonText(
    bool is_one_time) const {
  return @"";
}

}  // namespace permissions
