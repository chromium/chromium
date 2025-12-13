// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/ios/content/permission_prompt/permission_prompt_test_util.h"

#include "components/permissions/resolvers/permission_prompt_options.h"

namespace permissions {

MockPermissionPromptDelegate::MockPermissionPromptDelegate() = default;
MockPermissionPromptDelegate::~MockPermissionPromptDelegate() = default;

const std::vector<std::unique_ptr<PermissionRequest>>&
MockPermissionPromptDelegate::Requests() {
  return requests_;
}

GURL MockPermissionPromptDelegate::GetRequestingOrigin() const {
  return GURL("https://example.com");
}

GURL MockPermissionPromptDelegate::GetEmbeddingOrigin() const {
  return GURL("https://example.com");
}

void MockPermissionPromptDelegate::Accept() {
  accept_called_ = true;
}
void MockPermissionPromptDelegate::AcceptThisTime() {
  accept_this_time_called_ = true;
}
void MockPermissionPromptDelegate::Deny() {
  deny_called_ = true;
}
void MockPermissionPromptDelegate::Dismiss() {}
void MockPermissionPromptDelegate::Ignore() {}

GeolocationAccuracy
MockPermissionPromptDelegate::GetInitialGeolocationAccuracySelection() const {
  return GeolocationAccuracy::kPrecise;
}
void MockPermissionPromptDelegate::SetPromptOptions(
    PromptOptions prompt_options) {}
void MockPermissionPromptDelegate::FinalizeCurrentRequests() {}
void MockPermissionPromptDelegate::OpenHelpCenterLink(const ui::Event& event) {}
void MockPermissionPromptDelegate::PreIgnoreQuietPrompt() {}

std::optional<PermissionUiSelector::QuietUiReason>
MockPermissionPromptDelegate::ReasonForUsingQuietUi() const {
  return std::nullopt;
}

bool MockPermissionPromptDelegate::ShouldCurrentRequestUseQuietUI() const {
  return false;
}
bool MockPermissionPromptDelegate::ShouldDropCurrentRequestIfCannotShowQuietly()
    const {
  return false;
}
bool MockPermissionPromptDelegate::WasCurrentRequestAlreadyDisplayed() {
  return false;
}

void MockPermissionPromptDelegate::SetDismissOnTabClose() {}
void MockPermissionPromptDelegate::SetPromptShown() {}
void MockPermissionPromptDelegate::SetDecisionTime() {}
void MockPermissionPromptDelegate::SetManageClicked() {}
void MockPermissionPromptDelegate::SetLearnMoreClicked() {}
void MockPermissionPromptDelegate::SetHatsShownCallback(
    base::OnceCallback<void()> callback) {}

content::WebContents* MockPermissionPromptDelegate::GetAssociatedWebContents() {
  return nullptr;
}

base::WeakPtr<PermissionPrompt::Delegate>
MockPermissionPromptDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool MockPermissionPromptDelegate::RecreateView() {
  return false;
}

const PermissionPrompt* MockPermissionPromptDelegate::GetCurrentPrompt() const {
  return nullptr;
}

void MockPermissionPromptDelegate::AddRequest(
    std::unique_ptr<PermissionRequest> request) {
  requests_.push_back(std::move(request));
}

bool MockPermissionPromptDelegate::accept_called() const {
  return accept_called_;
}
bool MockPermissionPromptDelegate::accept_this_time_called() const {
  return accept_this_time_called_;
}
bool MockPermissionPromptDelegate::deny_called() const {
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
