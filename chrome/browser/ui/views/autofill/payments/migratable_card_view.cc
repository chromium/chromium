// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/migratable_card_view.h"

#include "base/functional/bind.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

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
                                                should_show_checkbox)
                   .release());

  checkbox_uncheck_text_container_ =
      AddChildView(views::Builder<views::View>()
                       .SetBackground(views::CreateThemedSolidBackground(
                           ui::kColorBubbleFooterBackground))
                       .Build());
  views::BoxLayout* layout = checkbox_uncheck_text_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::VH(provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_VERTICAL),
                          provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  checkbox_uncheck_text_container_->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CHECKBOX_UNCHECK_WARNING),
      CONTEXT_DIALOG_BODY_TEXT_SMALL, ChromeTextStyle::STYLE_RED));
  checkbox_uncheck_text_container_->SetVisible(false);
}

MigratableCardView::~MigratableCardView() = default;

bool MigratableCardView::GetSelected() const {
  return !checkbox_ || checkbox_->GetChecked();
}

std::string MigratableCardView::GetGuid() const {
  return migratable_credit_card_.credit_card().guid();
}

std::u16string MigratableCardView::GetCardIdentifierString() const {
  return migratable_credit_card_.credit_card().CardNameAndLastFourDigits();
}

std::unique_ptr<views::View>
MigratableCardView::GetMigratableCardDescriptionView(
    const MigratableCreditCard& migratable_credit_card,
    bool should_show_checkbox) {
  auto migratable_card_description_view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  migratable_card_description_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  std::unique_ptr<views::Label> card_description =
      std::make_unique<views::Label>(GetCardIdentifierString(),
                                     views::style::CONTEXT_LABEL);
  card_description->SetMultiLine(true);
  card_description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  constexpr int kCardDescriptionMaximumWidth = 260;
  card_description->SetMaximumWidth(kCardDescriptionMaximumWidth);

  constexpr int kMigrationResultImageSize = 16;
  switch (migratable_credit_card.migration_status()) {
    case MigratableCreditCard::MigrationStatus::UNKNOWN: {
      if (should_show_checkbox) {
        checkbox_ = migratable_card_description_view->AddChildView(
            std::make_unique<views::Checkbox>(
                std::u16string(),
                base::BindRepeating(&MigratableCardView::CheckboxPressed,
                                    base::Unretained(this))));
        checkbox_->SetChecked(true);
        // TODO(crbug.com/40586517): Currently the ink drop animation circle is
        // cropped by the border of scroll bar view. Find a way to adjust the
        // format.
        views::InkDrop::Get(checkbox_->ink_drop_view())
            ->SetMode(views::InkDropHost::InkDropMode::OFF);
        checkbox_->GetViewAccessibility().SetName(*card_description.get());
      }
      break;
    }
    case MigratableCreditCard::MigrationStatus::SUCCESS_ON_UPLOAD: {
      auto* migration_succeeded_image =
          migratable_card_description_view->AddChildView(
              std::make_unique<views::ImageView>());
      migration_succeeded_image->SetImage(ui::ImageModel::FromVectorIcon(
          vector_icons::kCheckCircleIcon, ui::kColorAlertLowSeverity,
          kMigrationResultImageSize));
      break;
    }
    case MigratableCreditCard::MigrationStatus::FAILURE_ON_UPLOAD: {
      auto* migration_failed_image =
          migratable_card_description_view->AddChildView(
              std::make_unique<views::ImageView>());
      migration_failed_image->SetImage(ui::ImageModel::FromVectorIcon(
          vector_icons::kErrorIcon, ui::kColorAlertHighSeverity,
          kMigrationResultImageSize));
      break;
    }
  }

  std::unique_ptr<views::View> card_network_and_last_four_digits =
      std::make_unique<views::View>();
  card_network_and_last_four_digits->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL_LIST)));

  std::unique_ptr<views::ImageView> card_image =
      std::make_unique<views::ImageView>();
  card_image->SetImage(
      ui::ImageModel::FromResourceId(CreditCard::IconResourceId(
          migratable_credit_card.credit_card().network())));
  card_image->GetViewAccessibility().SetName(
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
  card_expiration->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);
  card_expiration->SetMultiLine(true);
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
        views::CreateVectorImageButtonWithNativeTheme(
            base::BindRepeating(
                [](LocalCardMigrationDialogView* parent_dialog,
                   std::string guid) {
                  parent_dialog->DeleteCard(std::move(guid));
                },
                parent_dialog_, GetGuid()),
            kTrashCanIcon);
    delete_card_from_local_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TRASH_CAN_BUTTON_TOOLTIP));
    delete_card_from_local_button->SetFocusBehavior(
        FocusBehavior::ACCESSIBLE_ONLY);
    delete_card_from_local_button_ =
        migratable_card_description_view->AddChildView(
            std::move(delete_card_from_local_button));
  }

  return migratable_card_description_view;
}

void MigratableCardView::CheckboxPressed() {
  // If the button clicked is a checkbox. Enable/disable the save
  // button if needed.
  parent_dialog_->OnCardCheckboxToggled();
  // The warning text will be visible only when user unchecks the checkbox.
  checkbox_uncheck_text_container_->SetVisible(!checkbox_->GetChecked());
  InvalidateLayout();
  parent_dialog_->UpdateLayout();
}

BEGIN_METADATA(MigratableCardView)
ADD_READONLY_PROPERTY_METADATA(bool, Selected)
ADD_READONLY_PROPERTY_METADATA(std::string, Guid)
ADD_READONLY_PROPERTY_METADATA(std::u16string, CardIdentifierString)
END_METADATA

}  // namespace autofill
