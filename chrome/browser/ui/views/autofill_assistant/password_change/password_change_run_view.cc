// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_progress.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace {

// TODO(crbug.com/1322419): Where possible, replace these constants by values
// obtained from the global layout provider.
constexpr int kTopIconSize = 96;

// Helper method that creates a button container and sets the appropriate
// alignment and spacing.
std::unique_ptr<views::View> CreateButtonContainer() {
  auto container =
      views::Builder<views::View>()
          .SetID(static_cast<int>(
              PasswordChangeRunView::ChildrenViewsIds::kButtonContainer))
          .Build();
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::TLBR(/*top=*/0,
                            /*left=*/
                            views::LayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                            /*bottom=*/0, /*right=*/0));
  return container;
}

// Helper function that creates a button.
std::unique_ptr<views::MdTextButton> CreateButton(
    const PasswordChangeRunDisplay::PromptChoice& choice,
    views::Button::PressedCallback callback) {
  return views::Builder<views::MdTextButton>()
      .SetCallback(std::move(callback))
      .SetText(choice.text)
      .SetProminent(choice.highlighted)
      .Build();
}

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
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(
          views::LayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG))
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true))
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(
                      /*top=*/views::LayoutProvider::Get()->GetDistanceMetric(
                          views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                      /*left=*/0, /*bottom=*/0, /*right=*/0));

  top_icon_ = AddChildView(views::Builder<views::ImageView>().Build());

  // childrenIDsOffset makes sure that none of the IDs set in this view will
  // colapse with the ones insde `PasswordChangeRunProgress`.
  password_change_run_progress_ = AddChildView(
      std::make_unique<PasswordChangeRunProgress>(/*childrenIDsOffset=*/20));

  title_container_ = AddChildView(
      views::Builder<views::View>()
          .SetID(static_cast<int>(ChildrenViewsIds::kTitleContainer))
          .Build());
  title_container_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true));

  body_ = AddChildView(views::Builder<views::View>()
                           .SetID(static_cast<int>(ChildrenViewsIds::kBody))
                           .Build());
  body_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true))
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(
                      /*top=*/views::LayoutProvider::Get()->GetDistanceMetric(
                          views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                      /*left=*/0, /*bottom=*/0, /*right=*/0));
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
    autofill_assistant::password_change::ProgressStep progress_step) {
  password_change_run_progress_->SetProgressBarStep(progress_step);
}

void PasswordChangeRunView::ShowBasePrompt(
    const std::vector<PromptChoice>& choices) {
  DCHECK(body_);
  body_->RemoveAllChildViews();
  body_->AddChildView(std::make_unique<views::Separator>());
  views::View* button_container = body_->AddChildView(CreateButtonContainer());
  for (size_t index = 0; index < choices.size(); ++index) {
    if (!choices[index].text.empty()) {
      button_container->AddChildView(CreateButton(
          choices[index],
          base::BindRepeating(
              &PasswordChangeRunController::OnBasePromptChoiceSelected,
              controller_, index)));
    }
  }
}

void PasswordChangeRunView::ShowUseGeneratedPasswordPrompt(
    const std::u16string& title,
    const std::u16string& suggested_password,
    const std::u16string& description,
    const PromptChoice& manual_password_choice,
    const PromptChoice& generated_password_choice) {
  SetTitle(title);
  title_container_->AddChildView(
      views::Builder<views::Label>()
          .SetText(suggested_password)
          .SetTextStyle(views::style::STYLE_PRIMARY)
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetID(static_cast<int>(ChildrenViewsIds::kSuggestedPassword))
          .Build());

  SetDescription(description);

  DCHECK(body_);
  views::View* button_container = body_->AddChildView(CreateButtonContainer());
  button_container->AddChildView(CreateButton(
      manual_password_choice,
      base::BindRepeating(
          &PasswordChangeRunController::OnGeneratedPasswordSelected,
          controller_, false)));
  button_container->AddChildView(CreateButton(
      generated_password_choice,
      base::BindRepeating(
          &PasswordChangeRunController::OnGeneratedPasswordSelected,
          controller_, true)));
}

void PasswordChangeRunView::ClearPrompt() {
  DCHECK(body_);
  body_->RemoveAllChildViews();
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
