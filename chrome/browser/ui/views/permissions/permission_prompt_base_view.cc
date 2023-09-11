// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_base_view.h"

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

constexpr UrlIdentity::TypeSet allowed_types = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kChromeExtension,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kFile};

constexpr UrlIdentity::FormatOptions options = {
    .default_options = {
        UrlIdentity::DefaultFormatOptions::kOmitCryptographicScheme}};

}  // namespace

PermissionPromptBaseView::PermissionPromptBaseView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate)
    : url_identity_(GetUrlIdentity(browser, *delegate)) {
  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  SetDefaultButton(ui::DIALOG_BUTTON_NONE);
}

void PermissionPromptBaseView::AddedToWidget() {
  if (url_identity_.type == UrlIdentity::Type::kDefault) {
    // There is a risk of URL spoofing from origins that are too wide to fit in
    // the bubble; elide origins from the front to prevent this.
    GetBubbleFrameView()->SetTitleView(
        CreateTitleOriginLabel(GetWindowTitle()));
  }
}

bool PermissionPromptBaseView::ShouldIgnoreButtonPressedEventHandling(
    View* button,
    const ui::Event& event) const {
  // Ignore the key pressed event if the button row bounds intersect with PiP
  // windows bounds.
  if (!event.IsKeyEvent()) {
    return false;
  }

  absl::optional<gfx::Rect> pip_window_bounds =
      PictureInPictureWindowManager::GetInstance()
          ->GetPictureInPictureWindowBounds();

  return pip_window_bounds &&
         pip_window_bounds->Intersects(button->GetBoundsInScreen());
}

void PermissionPromptBaseView::FilterUnintenedEventsAndRunCallbacks(
    int button_id,
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(event)) {
    return;
  }

  View* button = AsDialogDelegate()->GetExtraView()->GetViewByID(button_id);

  if (ShouldIgnoreButtonPressedEventHandling(button, event)) {
    return;
  }

  RunButtonCallback(button_id);
}

// static
UrlIdentity PermissionPromptBaseView::GetUrlIdentity(
    Browser* browser,
    permissions::PermissionPrompt::Delegate& delegate) {
  DCHECK(!delegate.Requests().empty());
  GURL origin_url = delegate.GetRequestingOrigin();

  UrlIdentity url_identity =
      UrlIdentity::CreateFromUrl(browser ? browser->profile() : nullptr,
                                 origin_url, allowed_types, options);

  if (url_identity.type == UrlIdentity::Type::kFile) {
    // File URLs will show the same constant.
    url_identity.name =
        l10n_util::GetStringUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT_THIS_FILE);
  }

  return url_identity;
}

BEGIN_METADATA(PermissionPromptBaseView, views::BubbleDialogDelegateView)
END_METADATA
