// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/local_card_migration_dialog_view.h"

#include "base/location.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_factory.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_offer_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/ui/local_card_migration_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

LocalCardMigrationDialogView::LocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {}

LocalCardMigrationDialogView::~LocalCardMigrationDialogView() {}

void LocalCardMigrationDialogView::ShowDialog() {
  Init();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void LocalCardMigrationDialogView::CloseDialog() {
  GetWidget()->Close();
}

void LocalCardMigrationDialogView::OnMigrationFinished() {
  // TODO(crbug/867194): Add feedback and tip value prompt.
}

gfx::Size LocalCardMigrationDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType LocalCardMigrationDialogView::GetModalType() const {
  // This should be a modal dialog since we don't want users to lose progress
  // in the migration workflow until they are done.
  return ui::MODAL_TYPE_CHILD;
}

void LocalCardMigrationDialogView::AddedToWidget() {
  GetWidget()->AddObserver(this);
}

bool LocalCardMigrationDialogView::ShouldShowCloseButton() const {
  return false;
}

base::string16 LocalCardMigrationDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? GetOkButtonLabel()
                                        : GetCancelButtonLabel();
}

// TODO(crbug.com/867194): Update this method when adding feedback.
bool LocalCardMigrationDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  // If all checkboxes are unchecked, disable the save button.
  if (button == ui::DIALOG_BUTTON_OK) {
    DCHECK(offer_view_);
    return !offer_view_->GetSelectedCardGuids().empty();
  }
  return true;
}

bool LocalCardMigrationDialogView::Accept() {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      DCHECK(offer_view_);
      controller_->OnSaveButtonClicked(offer_view_->GetSelectedCardGuids());
      return true;
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      return true;
  }
}

bool LocalCardMigrationDialogView::Cancel() {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      controller_->OnCancelButtonClicked();
      return true;
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      controller_->OnViewCardsButtonClicked();
      return true;
  }
}

void LocalCardMigrationDialogView::OnWidgetClosing(views::Widget* widget) {
  controller_->OnDialogClosed();
  widget->RemoveObserver(this);
}

// TODO(crbug/867194): Add button pressed logic for kDeleteCardButtonTag.
void LocalCardMigrationDialogView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  // The button clicked is a checkbox. Enable/disable the save
  // button if needed.
  DCHECK_EQ(sender->GetClassName(), views::Checkbox::kViewClassName);
  DialogModelChanged();
}

void LocalCardMigrationDialogView::Init() {
  if (has_children())
    return;

  SetLayoutManager(std::make_unique<views::FillLayout>());
  offer_view_ = new LocalCardMigrationOfferView(controller_, this);
  AddChildView(offer_view_);
}

base::string16 LocalCardMigrationDialogView::GetOkButtonLabel() const {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_SAVE);
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_DONE);
  }
}

base::string16 LocalCardMigrationDialogView::GetCancelButtonLabel() const {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_CANCEL);
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_VIEW_CARDS);
  }
}

LocalCardMigrationDialog* CreateLocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents) {
  return new LocalCardMigrationDialogView(controller, web_contents);
}

}  // namespace autofill
