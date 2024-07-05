// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/compositor/layer.h"

ExclusiveAccessPermissionPrompt::ExclusiveAccessPermissionPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate),
      delegate_(delegate) {
  ShowPrompt();

  LocationBarView* lbv = GetLocationBarView();

  // Before showing a chip make sure the LocationBar is in a valid state. That
  // fixes a bug when a chip overlays the padlock icon.
  lbv->InvalidateLayout();
  lbv->GetChipController()->ShowPermissionChip(delegate->GetWeakPtr());
}

ExclusiveAccessPermissionPrompt::~ExclusiveAccessPermissionPrompt() {
  ClosePrompt();
}

permissions::PermissionPromptDisposition
ExclusiveAccessPermissionPrompt::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::CUSTOM_MODAL_DIALOG;
}

void ExclusiveAccessPermissionPrompt::DismissScrim() {
  delegate_->Dismiss();
}

base::WeakPtr<permissions::PermissionPrompt::Delegate>
ExclusiveAccessPermissionPrompt::GetPermissionPromptDelegate() const {
  return delegate_->GetWeakPtr();
}

ExclusiveAccessPermissionPromptView*
ExclusiveAccessPermissionPrompt::GetViewForTesting() {
  return static_cast<ExclusiveAccessPermissionPromptView*>(
      prompt_view_tracker_.view());
}

void ExclusiveAccessPermissionPrompt::ShowPrompt() {
  raw_ptr<ExclusiveAccessPermissionPromptView> prompt_view =
      new ExclusiveAccessPermissionPromptView(browser(),
                                              GetPermissionPromptDelegate());
  prompt_view_tracker_.SetView(prompt_view);
  content_scrim_widget_ =
      EmbeddedPermissionPromptContentScrimView::CreateScrimWidget(
          weak_factory_.GetWeakPtr(),
          web_contents()->GetColorProvider().GetColor(ui::kColorSysStateScrim));
  content_scrim_widget_->GetContentsView()->SetPaintToLayer(ui::LAYER_TEXTURED);
  content_scrim_widget_->GetContentsView()->layer()->SetBackgroundBlur(4.0f);
  prompt_view->UpdateAnchor(content_scrim_widget_.get());
  prompt_view->Show();
}

void ExclusiveAccessPermissionPrompt::ClosePrompt() {
  if (auto* prompt_view = static_cast<ExclusiveAccessPermissionPromptView*>(
          prompt_view_tracker_.view())) {
    prompt_view->PrepareToClose();
    prompt_view->GetWidget()->Close();
    prompt_view_tracker_.SetView(nullptr);
  }

  if (content_scrim_widget_) {
    content_scrim_widget_->Close();
    content_scrim_widget_.reset();
  }
}
