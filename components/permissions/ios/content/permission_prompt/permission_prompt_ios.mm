// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/ios/content/permission_prompt/permission_prompt_ios.h"

#include "base/notimplemented.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

PermissionPromptIOS::PermissionPromptIOS(content::WebContents* web_contents,
                                         Delegate* delegate)
    : web_contents_(web_contents), delegate_(delegate) {
  DCHECK(web_contents_);
  DCHECK(delegate_);
  std::transform(delegate_->Requests().begin(), delegate_->Requests().end(),
                 std::back_inserter(requests_),
                 [](const std::unique_ptr<PermissionRequest>& request_ptr) {
                   return request_ptr->GetWeakPtr();
                 });
}

PermissionPromptIOS::~PermissionPromptIOS() = default;

bool PermissionPromptIOS::UpdateAnchor() {
  NOTIMPLEMENTED();
  return false;
}

PermissionPrompt::TabSwitchingBehavior
PermissionPromptIOS::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kKeepPromptAlive;
}

std::optional<gfx::Rect> PermissionPromptIOS::GetViewBoundsInScreen() const {
  return std::nullopt;
}

bool PermissionPromptIOS::ShouldFinalizeRequestAfterDecided() const {
  return true;
}

std::vector<permissions::ElementAnchoredBubbleVariant>
PermissionPromptIOS::GetPromptVariants() const {
  return {};
}

bool PermissionPromptIOS::IsAskPrompt() const {
  return true;
}

std::optional<permissions::feature_params::PermissionElementPromptPosition>
PermissionPromptIOS::GetPromptPosition() const {
  return std::nullopt;
}

void PermissionPromptIOS::Accept() {
  delegate_->Accept();
}

void PermissionPromptIOS::AcceptThisTime() {
  delegate_->AcceptThisTime();
}

void PermissionPromptIOS::Deny() {
  delegate_->Deny();
}

NSString* PermissionPromptIOS::GetPositiveButtonText(bool is_one_time) const {
  return @"";
}

NSString* PermissionPromptIOS::GetNegativeButtonText(bool is_one_time) const {
  return @"";
}

NSString* PermissionPromptIOS::GetPositiveEphemeralButtonText(
    bool is_one_time) const {
  return @"";
}

size_t PermissionPromptIOS::PermissionCount() const {
  return Requests().size();
}

ContentSettingsType PermissionPromptIOS::GetContentSettingType(
    size_t position) const {
  const std::vector<base::WeakPtr<PermissionRequest>>& requests = Requests();
  CHECK_LT(position, requests.size());
  return requests[position]->GetContentSettingsType();
}

static bool IsValidMediaRequestGroup(
    const std::vector<base::WeakPtr<PermissionRequest>>& requests) {
  if (requests.size() < 2 || !requests[0] || !requests[1]) {
    return false;
  }
  return ((requests[0]->request_type() == RequestType::kMicStream &&
           requests[1]->request_type() == RequestType::kCameraStream) ||
          (requests[0]->request_type() == RequestType::kCameraStream &&
           requests[1]->request_type() == RequestType::kMicStream));
}

void PermissionPromptIOS::CheckValidRequestGroup(
    const std::vector<base::WeakPtr<PermissionRequest>>& requests) const {
  DCHECK_EQ(static_cast<size_t>(2u), requests.size());
  DCHECK((IsValidMediaRequestGroup(requests)));
}

PermissionRequest::AnnotatedMessageText
PermissionPromptIOS::GetAnnotatedMessageText() const {
  const std::vector<base::WeakPtr<PermissionRequest>>& requests = Requests();
  if (requests.size() == 1) {
    return requests[0]->GetDialogAnnotatedMessageText(
        delegate_->GetEmbeddingOrigin());
  }
  CheckValidRequestGroup(requests);

  // We only end up here if 2 requests are combined in one prompt (which only
  // happens for Audio & Video). All other requests are handled in the if block
  // above. For Audio and Video (which can be allowed once), format origins
  // bold.
  return PermissionRequest::GetDialogAnnotatedMessageText(
      url_formatter::FormatUrlForSecurityDisplay(
          delegate_->GetRequestingOrigin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
      IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_INFOBAR_TEXT,
      /*format_origin_bold=*/true);
}

GURL PermissionPromptIOS::GetRequestingOrigin() const {
  return delegate_->GetRequestingOrigin();
}

const std::vector<base::WeakPtr<permissions::PermissionRequest>>&
PermissionPromptIOS::Requests() const {
  return requests_;
}

}  // namespace permissions
