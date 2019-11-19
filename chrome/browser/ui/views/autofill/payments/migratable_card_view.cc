// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/migratable_card_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_dialog_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

constexpr char MigratableCardView::kViewClassName[] = "MigratableCardView";

MigratableCardView::MigratableCardView(
    const MigratableCreditCard& migratable_credit_card,
    LocalCardMigrationDialogView* parent_dialog,
    bool should_show_checkbox)
    : migratable_credit_card_(migratable_credit_card),
      parent_dialog_(parent_dialog) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI)));

  AddChildView(GetMigratableCardDescriptionView(migratable_credit_card,
                                                should_show_checkbox, this)
                   .release());

  checkbox_uncheck_text_container_ = new views::View();
  views::BoxLayout* layout = checkbox_uncheck_text_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(provider->GetDistanceMetric(
                          views::DISTANCE_RELATED_CONTROL_VERTICAL),
                      provider->GetDistanceMetric(
                          views::DISTANCE_RELATED_CONTROL_HORIZONTAL)),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  views::Label* checkbox_uncheck_text_ = new views::Label(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CHECKBOX_UNCHECK_WARNING),
      CONTEXT_BODY_TEXT_SMALL, ChromeTextStyle::STYLE_RED);

  checkbox_uncheck_text_container_->AddChildView(checkbox_uncheck_text_);
  checkbox_uncheck_text_container_->SetBackground(
      views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_BubbleFooterBackground)));
  checkbox_uncheck_text_container_->SetVisible(false);

  AddChildView(checkbox_uncheck_text_container_);
}

MigratableCardView::~MigratableCardView() = default;

bool MigratableCardView::IsSelected() {
  return !checkbox_ || checkbox_->GetChecked();
}

std::string MigratableCardView::GetGuid() {
  return migratable_credit_card_.credit_card().guid();
}

const base::string16 MigratableCardView::GetNetworkAndLastFourDigits() const {
  return migratable_credit_card_.credit_card().NetworkAndLastFourDigits();
}

std::unique_ptr<views::View>
MigratableCardView::GetMigratableCardDescriptionView(
    const MigratableCreditCard& migratable_credit_card,
    bool should_show_checkbox,
    ButtonListener* listener) {
  auto migratable_card_description_view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  migratable_card_description_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  std::unique_ptr<views::Label> card_description =
      std::make_unique<views::Label>(
          migratable_credit_card.credit_card().NetworkAndLastFourDigits(),
          views::style::CONTEXT_LABEL);

  constexpr int kMigrationResultImageSize = 16;
  switch (migratable_credit_card.migration_status()) {
    case MigratableCreditCard::MigrationStatus::UNKNOWN: {
      if (should_show_checkbox) {
        checkbox_ = new views::Checkbox(base::string16(), listener);
        checkbox_->SetChecked(true);
        // TODO(crbug/867194): Currently the ink drop animation circle is
        // cropped by the border of scroll bar view. Find a way to adjust the
        // format.
        checkbox_->SetInkDropMode(views::InkDropHostView::InkDropMode::OFF);
        checkbox_->SetAssociatedLabel(card_description.get());
        migratable_card_description_view->AddChildView(checkbox_);
      }
      break;
    }
    case MigratableCreditCard::MigrationStatus::SUCCESS_ON_UPLOAD: {
      auto* migration_succeeded_image = new views::ImageView();
      migration_succeeded_image->SetImage(gfx::CreateVectorIcon(
          vector_icons::kCheckCircleIcon, kMigrationResultImageSize,
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_AlertSeverityLow)));
      migratable_card_description_view->AddChildView(migration_succeeded_image);
      break;
    }
    case MigratableCreditCard::MigrationStatus::FAILURE_ON_UPLOAD: {
      auto* migration_failed_image = new views::ImageView();
      migration_failed_image->SetImage(gfx::CreateVectorIcon(
          vector_icons::kErrorIcon, kMigrationResultImageSize,
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_AlertSeverityHigh)));
      migratable_card_description_view->AddChildView(migration_failed_image);
      break;
    }
  }

  std::unique_ptr<views::View> card_network_and_last_four_digits =
      std::make_unique<views::View>();
  card_network_and_last_four_digits->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL_LIST)));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<views::ImageView> card_image =
      std::make_unique<views::ImageView>();
  card_image->SetImage(
      rb.GetImageNamed(CreditCard::IconResourceId(
                           migratable_credit_card.credit_card().network()))
          .AsImageSkia());
  card_image->SetAccessibleName(
      migratable_credit_card.credit_card().NetworkForDisplay());
  card_network_and_last_four_digits->AddChildView(card_image.release());
  card_network_and_last_four_digits->AddChildView(card_description.release());
  migratable_card_description_view->AddChildView(
      card_network_and_last_four_digits.release());

  std::unique_ptr<views::Label> card_expiration =
      std::make_unique<views::Label>(
          migratable_credit_card.credit_card()
              .AbbreviatedExpirationDateForDisplay(/*with_prefix=*/true),
          views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  migratable_card_description_view->AddChildView(card_expiration.release());

  // If card is not successfully uploaded we show the invalid card
  // label and the trash can icon.
  if (migratable_credit_card.migration_status() ==
      MigratableCreditCard::MigrationStatus::FAILURE_ON_UPLOAD) {
    migratable_card_description_view->AddChildView(new views::Label(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_LABEL_INVALID_CARDS),
        views::style::CONTEXT_LABEL, ChromeTextStyle::STYLE_RED));

    auto delete_card_from_local_button =
        views::CreateVectorImageButton(listener);
    views::SetImageFromVectorIcon(delete_card_from_local_button.get(),
                                  kTrashCanIcon);
    delete_card_from_local_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TRASH_CAN_BUTTON_TOOLTIP));
    delete_card_from_local_button_ =
        migratable_card_description_view->AddChildView(
            std::move(delete_card_from_local_button));
  }

  return migratable_card_description_view;
}

const char* MigratableCardView::GetClassName() const {
  return kViewClassName;
}

void MigratableCardView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == checkbox_) {
    // If the button clicked is a checkbox. Enable/disable the save
    // button if needed.
    parent_dialog_->DialogModelChanged();
    // The warning text will be visible only when user unchecks the checkbox.
    checkbox_uncheck_text_container_->SetVisible(!checkbox_->GetChecked());
    InvalidateLayout();
    parent_dialog_->UpdateLayout();
  } else {
    // Otherwise it is the trash can button clicked. Delete the corresponding
    // card from local storage.
    DCHECK_EQ(sender, delete_card_from_local_button_);
    parent_dialog_->DeleteCard(GetGuid());
  }
}

}  // namespace autofill
