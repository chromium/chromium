// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/local_card_migration_offer_view.h"

#include "base/location.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/migratable_card_view.h"
#include "chrome/browser/ui/views/autofill/view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/ui/local_card_migration_dialog_controller.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

MigratableCardView* AsMigratableCardView(views::View* view) {
  DCHECK_EQ(MigratableCardView::kViewClassName, view->GetClassName());
  return static_cast<MigratableCardView*>(view);
}

}  // namespace

LocalCardMigrationOfferView::LocalCardMigrationOfferView(
    LocalCardMigrationDialogController* controller,
    views::ButtonListener* listener)
    : controller_(controller) {
  Init(listener);
}

LocalCardMigrationOfferView::~LocalCardMigrationOfferView() {}

void LocalCardMigrationOfferView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  if (!controller_)
    return;

  controller_->OnLegalMessageLinkClicked(
      legal_message_container_->GetUrlForLink(label, range));
}

void LocalCardMigrationOfferView::Init(views::ButtonListener* listener) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // Set up main contents container.
  constexpr int kMainContainerChildSpacing = 24;
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), kMainContainerChildSpacing));

  // TODO(siyua@): Move image_container to LocalCardMigrationDialogView after
  // introducing other feedback child views since it is shared between them.
  std::unique_ptr<views::View> image_container =
      std::make_unique<views::View>();
  image_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  constexpr int kImageBorderBottom = 8;
  image_container->SetBorder(
      views::CreateEmptyBorder(0, 0, kImageBorderBottom, 0));
  std::unique_ptr<views::ImageView> image =
      std::make_unique<views::ImageView>();
  image->SetImage(rb.GetImageSkiaNamed(IDR_AUTOFILL_MIGRATION_DIALOG_HEADER));
  image->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME));
  image_container->AddChildView(image.release());
  AddChildView(image_container.release());

  // TODO(siyua@): Move title to LocalCardMigrationDialogView.
  std::unique_ptr<views::Label> title =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_OFFER));
  constexpr int kMigrationDialogTitleFontSize = 8;
  title->SetFontList(gfx::FontList().Derive(kMigrationDialogTitleFontSize,
                                            gfx::Font::NORMAL,
                                            gfx::Font::Weight::MEDIUM));
  title->SetEnabledColor(gfx::kGoogleGrey900);
  constexpr int kMigrationDialogTitleLineHeight = 20;
  title->SetLineHeight(kMigrationDialogTitleLineHeight);
  AddChildView(title.release());

  std::unique_ptr<views::View> contents_container =
      std::make_unique<views::View>();
  contents_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  constexpr int kMigrationDialogInsets = 24;
  gfx::Insets migration_dialog_insets = gfx::Insets(0, kMigrationDialogInsets);
  contents_container->SetBorder(
      views::CreateEmptyBorder(migration_dialog_insets));

  std::unique_ptr<views::Label> explanation_text =
      std::make_unique<views::Label>(
          l10n_util::GetPluralStringFUTF16(
              IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_OFFER,
              controller_->GetCardList().size()),
          CONTEXT_BODY_TEXT_LARGE, ChromeTextStyle::STYLE_SECONDARY);
  explanation_text->SetMultiLine(true);
  explanation_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  contents_container->AddChildView(explanation_text.release());

  card_list_view_ = new views::View();
  views::BoxLayout* card_list_view_layout =
      card_list_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  card_list_view_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  const std::vector<MigratableCreditCard>& migratable_credit_cards =
      controller_->GetCardList();
  for (size_t index = 0; index < migratable_credit_cards.size(); index++) {
    card_list_view_->AddChildView(
        new MigratableCardView(migratable_credit_cards[index], listener,
                               migratable_credit_cards.size() != 1));
  }

  std::unique_ptr<views::ScrollView> card_list_scroll_bar =
      std::make_unique<views::ScrollView>();
  card_list_scroll_bar->set_hide_horizontal_scrollbar(true);
  card_list_scroll_bar->SetContents(card_list_view_);
  card_list_scroll_bar->set_draw_overflow_indicator(false);
  constexpr int kCardListScrollViewHeight = 140;
  card_list_scroll_bar->ClipHeightTo(0, kCardListScrollViewHeight);
  contents_container->AddChildView(card_list_scroll_bar.release());
  AddChildView(contents_container.release());

  AddChildView(new views::Separator());

  legal_message_container_ =
      new LegalMessageView(controller_->GetLegalMessageLines(), this);
  constexpr int kContentBottomMargin = 48;
  legal_message_container_->SetBorder(views::CreateEmptyBorder(
      0, migration_dialog_insets.left(), kContentBottomMargin,
      migration_dialog_insets.right()));
  AddChildView(legal_message_container_);
}

const std::vector<std::string>
LocalCardMigrationOfferView::GetSelectedCardGuids() const {
  std::vector<std::string> selected_cards;
  for (int index = 0; index < card_list_view_->child_count(); ++index) {
    MigratableCardView* card =
        AsMigratableCardView(card_list_view_->child_at(index));
    if (card->IsSelected())
      selected_cards.push_back(card->GetGuid());
  }
  return selected_cards;
}

}  // namespace autofill
