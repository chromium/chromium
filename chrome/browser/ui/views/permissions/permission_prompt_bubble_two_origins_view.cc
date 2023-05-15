// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_two_origins_view.h"

#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"

namespace {

absl::optional<std::u16string> GetExtraTextTwoOrigin(
    permissions::PermissionPrompt::Delegate& delegate) {
  CHECK_GT(delegate.Requests().size(), 0u);
  switch (delegate.Requests()[0]->request_type()) {
    case permissions::RequestType::kStorageAccess:
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_EXPLANATION,
          url_formatter::FormatUrlForSecurityDisplay(
              delegate.GetRequestingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    default:
      NOTREACHED_NORETURN();
  }
}

std::u16string GetWindowTitleTwoOrigin(
    permissions::PermissionPrompt::Delegate& delegate) {
  CHECK_GT(delegate.Requests().size(), 0u);
  switch (delegate.Requests()[0]->request_type()) {
    case permissions::RequestType::kStorageAccess:
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_PROMPT_TITLE,
          url_formatter::FormatUrlForSecurityDisplay(
              delegate.GetRequestingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          url_formatter::FormatUrlForSecurityDisplay(
              delegate.GetEmbeddingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

PermissionPromptBubbleTwoOriginsView::PermissionPromptBubbleTwoOriginsView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    base::TimeTicks permission_requested_time,
    PermissionPromptStyle prompt_style)
    : PermissionPromptBubbleBaseView(browser,
                                     delegate,
                                     permission_requested_time,
                                     prompt_style,
                                     GetWindowTitleTwoOrigin(*delegate),
                                     GetWindowTitleTwoOrigin(*delegate),
                                     GetExtraTextTwoOrigin(*delegate)) {
  // Only requests for SAA should use this prompt.
  CHECK(delegate);
  CHECK_GT(delegate->Requests().size(), 0u);
  CHECK_EQ(delegate->Requests()[0]->request_type(),
           permissions::RequestType::kStorageAccess);

  // TODO(crbug/1433644): Call favicon factory and create favicon custom row.
}

PermissionPromptBubbleTwoOriginsView::~PermissionPromptBubbleTwoOriginsView() =
    default;

void PermissionPromptBubbleTwoOriginsView::AddedToWidget() {
  if (GetUrlIdentityObject().type == UrlIdentity::Type::kDefault) {
    // TODO(crbug/1433644): There might be a risk of URL spoofing from origins
    // that are too wide to fit in the bubble.
    std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
        GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetCollapseWhenHidden(true);
    label->SetMultiLine(true);
    label->SetMaxLines(4);
    GetBubbleFrameView()->SetTitleView(std::move(label));
  }
}
