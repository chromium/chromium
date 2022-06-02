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
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
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
  title_container_ = AddChildView(
      views::Builder<views::View>()
          .SetID(static_cast<int>(ChildrenViewsIds::kTitleContainer))
          .SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical))
          .Build());

  body_ = AddChildView(views::Builder<views::View>()
                           .SetID(static_cast<int>(ChildrenViewsIds::kBody))
                           .Build());
  auto* body_layout_manager =
      body_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  body_layout_manager->set_between_child_spacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
}

void PasswordChangeRunView::SetTopIcon(
    autofill_assistant::password_change::TopIcon top_icon) {
  DCHECK(top_icon_);
  top_icon_->SetImage(gfx::CreateVectorIcon(
      GetApcTopIconFromEnum(top_icon), kTopIconSize, gfx::kPlaceholderColor));
}

void PasswordChangeRunView::SetTitle(const std::u16string& title) {
  title_container_->RemoveAllChildViews();
  title_container_->AddChildView(
      views::Builder<views::Label>()
          .SetText(title)
          .SetMultiLine(true)
          .SetTextStyle(views::style::STYLE_PRIMARY)
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetID(static_cast<int>(ChildrenViewsIds::kTitle))
          .Build());
}
void PasswordChangeRunView::SetDescription(const std::u16string& description) {
  body_->RemoveAllChildViews();
  if (description.empty()) {
    return;
  }
  body_->AddChildView(std::make_unique<views::Separator>());
  body_->AddChildView(
      views::Builder<views::Label>()
          .SetText(description)
          .SetMultiLine(true)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetID(static_cast<int>(ChildrenViewsIds::kDescription))
          .Build());
}
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
