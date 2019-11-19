// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_views.h"

#include <utility>

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {

std::unique_ptr<views::View> CreateExtraView(views::ButtonListener* listener) {
  auto help_button = CreateVectorImageButton(listener);
  help_button->SetFocusForPlatform();
  help_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  SetImageFromVectorIcon(help_button.get(), vector_icons::kHelpOutlineIcon);
  return help_button;
}

}  // namespace

ConfirmBubbleViews::ConfirmBubbleViews(
    std::unique_ptr<ConfirmBubbleModel> model)
    : model_(std::move(model)), help_button_(nullptr) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_OK));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL,
      model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_CANCEL));
  help_button_ = DialogDelegate::SetExtraView(::CreateExtraView(this));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Use a fixed maximum message width, so longer messages will wrap.
  const int kMaxMessageWidth = 400;
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                kMaxMessageWidth, false);

  // Add the message label.
  auto label = std::make_unique<views::Label>(
      model_->GetMessageText(), views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  DCHECK(!label->GetText().empty());
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  label->SizeToFit(kMaxMessageWidth);
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  label_ = layout->AddView(std::move(label));

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CONFIRM_BUBBLE);
}

ConfirmBubbleViews::~ConfirmBubbleViews() {
}

bool ConfirmBubbleViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return !!(model_->GetButtons() & ConfirmBubbleModel::BUTTON_OK);
    case ui::DIALOG_BUTTON_CANCEL:
      return !!(model_->GetButtons() & ConfirmBubbleModel::BUTTON_CANCEL);
    default:
      NOTREACHED();
      return false;
  }
}

bool ConfirmBubbleViews::Cancel() {
  model_->Cancel();
  return true;
}

bool ConfirmBubbleViews::Accept() {
  model_->Accept();
  return true;
}

ui::ModalType ConfirmBubbleViews::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 ConfirmBubbleViews::GetWindowTitle() const {
  return model_->GetTitle();
}

bool ConfirmBubbleViews::ShouldShowCloseButton() const {
  return false;
}

void ConfirmBubbleViews::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == help_button_) {
    model_->OpenHelpPage();
    GetWidget()->Close();
  }
}

void ConfirmBubbleViews::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this && GetWidget()) {
    GetWidget()->GetRootView()->GetViewAccessibility().OverrideDescribedBy(
        label_);
  }
  DialogDelegateView::ViewHierarchyChanged(details);
}

namespace chrome {

void ShowConfirmBubble(gfx::NativeWindow window,
                       gfx::NativeView anchor_view,
                       const gfx::Point& origin,
                       std::unique_ptr<ConfirmBubbleModel> model) {
  constrained_window::CreateBrowserModalDialogViews(
      new ConfirmBubbleViews(std::move(model)), window)
      ->Show();
}

}  // namespace chrome
