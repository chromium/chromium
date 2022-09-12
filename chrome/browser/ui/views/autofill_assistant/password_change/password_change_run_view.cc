// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_progress.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

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
    const std::u16string text,
    bool highlighted,
    views::Button::PressedCallback callback) {
  return views::Builder<views::MdTextButton>()
      .SetCallback(std::move(callback))
      .SetText(text)
      .SetProminent(highlighted)
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
                                   /*adjust_height_for_width=*/true))
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(
                      /*top=*/views::LayoutProvider::Get()->GetDistanceMetric(
                          views::DISTANCE_RELATED_CONTROL_VERTICAL),
                      /*left=*/0, /*bottom=*/0, /*right=*/0));

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
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
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

autofill_assistant::password_change::ProgressStep
PasswordChangeRunView::GetProgressStep() {
  return password_change_run_progress_->GetCurrentProgressBarStep();
}

void PasswordChangeRunView::ShowBasePrompt(
    const std::u16string& description,
    const std::vector<PromptChoice>& choices) {
  DCHECK(body_);

  SetDescription(description);
  CreateBasePromptOptions(choices);
  password_change_run_progress_->PauseIconAnimation();
}

void PasswordChangeRunView::ShowBasePrompt(
    const std::vector<PromptChoice>& choices) {
  DCHECK(body_);

  body_->RemoveAllChildViews();
  // Do not create the separator if all choices have empty text.
  if (base::ranges::all_of(choices, [](const PromptChoice& choice) {
        return choice.text.empty();
      })) {
    return;
  }

  body_->AddChildView(std::make_unique<views::Separator>());

  CreateBasePromptOptions(choices);
  password_change_run_progress_->PauseIconAnimation();
}

void PasswordChangeRunView::CreateBasePromptOptions(
    const std::vector<PromptChoice>& choices) {
  views::View* button_container = body_->AddChildView(CreateButtonContainer());
  for (size_t index = 0; index < choices.size(); ++index) {
    if (!choices[index].text.empty()) {
      button_container->AddChildView(CreateButton(
          choices[index].text, choices[index].highlighted,
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
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetID(static_cast<int>(ChildrenViewsIds::kSuggestedPassword))
          .Build());

  SetDescription(description);
  password_change_run_progress_->PauseIconAnimation();

  DCHECK(body_);
  views::View* button_container = body_->AddChildView(CreateButtonContainer());
  button_container->AddChildView(CreateButton(
      manual_password_choice.text, false,
      base::BindRepeating(
          &PasswordChangeRunController::OnGeneratedPasswordSelected,
          controller_, false)));
  button_container->AddChildView(CreateButton(
      generated_password_choice.text, true,
      base::BindRepeating(
          &PasswordChangeRunController::OnGeneratedPasswordSelected,
          controller_, true)));
}

void PasswordChangeRunView::ShowStartingScreen(const GURL& url) {
  SetTopIcon(autofill_assistant::password_change::TopIcon::
                 TOP_ICON_OPEN_SITE_SETTINGS);

  const std::u16string formatted_url = url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitHTTP | url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlTrimAfterHost,
      base::UnescapeRule::SPACES, /*new_parsed=*/nullptr,
      /*prefix_end=*/nullptr, /*offset_for_adjustment=*/nullptr);
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_STARTING_SCREEN_TITLE,
      formatted_url));
  SetDescription(std::u16string());
}

void PasswordChangeRunView::ShowErrorScreen() {
  password_change_run_progress_->PauseIconAnimation();
  SetTopIcon(
      autofill_assistant::password_change::TopIcon::TOP_ICON_ERROR_OCCURRED);
  SetTitle(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ERROR_SCREEN_TITLE));
  SetDescription(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ERROR_SCREEN_DESCRIPTION));
}

void PasswordChangeRunView::ShowCompletionScreen(
    base::RepeatingClosure done_button_callback) {
  show_completion_screen_done_button_callback_ =
      std::move(done_button_callback);

  password_change_run_progress_->SetAnimationEndedCallback(base::BindOnce(
      &PasswordChangeRunView::OnShowCompletionScreen, base::Unretained(this)));
}

void PasswordChangeRunView::OnShowCompletionScreen() {
  SetTopIcon(
      autofill_assistant::password_change::TopIcon::TOP_ICON_CHANGED_PASSWORD);
  password_change_run_progress_->SetVisible(false);
  SetTitle(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_SUCCESSFULLY_CHANGED_PASSWORD_TITLE));

  const std::u16string password_manager_link = l10n_util::GetStringUTF16(
      IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT);
  size_t offset = 0;
  std::u16string description = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_SUCCESSFULLY_CHANGED_PASSWORD_DESCRIPTION,
      password_manager_link, &offset);

  body_->RemoveAllChildViews();
  body_->AddChildView(std::make_unique<views::Separator>());
  views::StyledLabel* description_view = body_->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetText(description)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetID(static_cast<int>(ChildrenViewsIds::kDescription))
          .Build());
  description_view->AddStyleRange(
      gfx::Range(offset, offset + password_manager_link.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PasswordChangeRunController::OpenPasswordManager, controller_)));

  views::View* button_container = body_->AddChildView(CreateButtonContainer());
  button_container->AddChildView(CreateButton(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_SUCCESSFULLY_CHANGED_PASSWORD_CLOSE_SIDE_PANEL),
      true, show_completion_screen_done_button_callback_));
}

void PasswordChangeRunView::ClearPrompt() {
  DCHECK(body_);
  body_->RemoveAllChildViews();
  password_change_run_progress_->ResumeIconAnimation();
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
