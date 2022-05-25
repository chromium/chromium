// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_view.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

PasswordChangeRunView::PasswordChangeRunView(
    base::WeakPtr<PasswordChangeRunController> controller,
    raw_ptr<AssistantDisplayDelegate> display_delegate)
    : controller_(controller), display_delegate_(display_delegate) {
  DCHECK(display_delegate_);

  // Renders the view in the display delegate and passes ownership of `this`.
  display_delegate_->SetView(base::WrapUnique(this));
}

PasswordChangeRunView::~PasswordChangeRunView() = default;

void PasswordChangeRunView::Show() {
  PasswordChangeRunView::CreateView();
}

void PasswordChangeRunView::CreateView() {
  DCHECK(controller_);
  // TODO(crbug.com/1322419): Add proper icons, sizes, style etc.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  // Set up top container.
  std::unique_ptr<views::View> top_container = std::make_unique<views::View>();
  top_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets()));

  top_icon_ = top_container->AddChildView(std::make_unique<views::ImageView>());
  top_icon_->SetImage(
      ui::ImageModel::FromVectorIcon(kKeyIcon, ui::kColorIcon, 64));
  top_icon_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  // TODO(brunobraga): Initialise progress bar.
  title_ = AddChildView(std::make_unique<views::Label>());
  description_ = AddChildView(std::make_unique<views::Label>());
  body_ = AddChildView(std::make_unique<views::View>());

  AddChildView(std::move(top_container));
}

// TODO(crbug.com/1322419): Implement set methods.
void PasswordChangeRunView::SetTopIcon(
    autofill_assistant::password_change::TopIcon top_icon) {}
void PasswordChangeRunView::SetTitle(std::u16string body_title) {}
void PasswordChangeRunView::SetDescription(
    std::u16string progress_description) {}
void PasswordChangeRunView::SetProgressBarStep(
    autofill_assistant::password_change::ProgressStep progress_step) {}

void PasswordChangeRunView::OnControllerGone() {
  Close();
}

void PasswordChangeRunView::Close() {
  // Remove this view from the `display_delegate_`, effectively destroying it.
  display_delegate_->RemoveView();
}

base::WeakPtr<PasswordChangeRunView> PasswordChangeRunView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(PasswordChangeRunView, views::View)
END_METADATA
