// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_view.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"
#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace {

// TODO(crbug.com/1322419): Where possible, replace these constants by values
// obtained from the global layout provider.
constexpr int kTopIconSize = 96;

}  // namespace

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
  // TODO(crbug.com/1322419): Add IDs to elements.
  DCHECK(controller_);
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_inside_border_insets(
      views::LayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  layout->set_between_child_spacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  top_icon_ = AddChildView(views::Builder<views::ImageView>().Build());

  // TODO(crbug.com/1322419): Adjust style (multiline, font style, etc.)
  title_ = AddChildView(std::make_unique<views::Label>());

  // TODO(crbug.com/1322419): Add and initialize progress bar.

  // TODO(crbug.com/1322419): Adjust style (multiline, font style, etc.)
  description_ = AddChildView(std::make_unique<views::Label>());

  // TODO(crbug.com/1322419): Add appropriate layout manager.
  body_ = AddChildView(std::make_unique<views::View>());
}

void PasswordChangeRunView::SetTopIcon(
    autofill_assistant::password_change::TopIcon top_icon) {
  DCHECK(top_icon_);
  top_icon_->SetImage(gfx::CreateVectorIcon(
      GetApcTopIconFromEnum(top_icon), kTopIconSize, gfx::kPlaceholderColor));
}

// TODO(crbug.com/1322419): Implement set methods.
void PasswordChangeRunView::SetTitle(const std::u16string& body_title) {}

void PasswordChangeRunView::SetDescription(
    const std::u16string& progress_description) {}

void PasswordChangeRunView::SetProgressBarStep(
    autofill_assistant::password_change::ProgressStep progress_step) {}

void PasswordChangeRunView::ShowBasePrompt(
    const std::vector<std::string>& options) {
  controller_->OnBasePromptOptionSelected(0);
}

void PasswordChangeRunView::ShowSuggestedPasswordPrompt(
    const std::u16string& suggested_password) {
  controller_->OnSuggestedPasswordSelected(true);
}

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
