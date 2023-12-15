// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MIGRATABLE_CARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MIGRATABLE_CARD_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Checkbox;
class ImageButton;
}  // namespace views

namespace autofill {

enum class LocalCardMigrationDialogState;
class LocalCardMigrationDialogView;
class MigratableCreditCard;

// A view composed of a checkbox or an image indicating migration results, card
// network image, card network, last four digits of card number and card
// expiration date. Used by LocalCardMigrationDialogView.
class MigratableCardView : public views::View {
  METADATA_HEADER(MigratableCardView, views::View)

 public:
  MigratableCardView(const MigratableCreditCard& migratable_credit_card,
                     LocalCardMigrationDialogView* parent_dialog,
                     bool should_show_checkbox);
  MigratableCardView(const MigratableCardView&) = delete;
  MigratableCardView& operator=(const MigratableCardView&) = delete;
  ~MigratableCardView() override;

  bool GetSelected() const;
  std::string GetGuid() const;
  std::u16string GetCardIdentifierString() const;

 private:
  std::unique_ptr<views::View> GetMigratableCardDescriptionView(
      const MigratableCreditCard& migratable_credit_card,
      bool should_show_checkbox);

  void CheckboxPressed();

  const MigratableCreditCard migratable_credit_card_;

  // The checkbox_ can remain null if the card list in the local
  // card migration dialog contains only one card.
  raw_ptr<views::Checkbox> checkbox_ = nullptr;

  raw_ptr<views::View> checkbox_uncheck_text_container_ = nullptr;

  raw_ptr<views::ImageButton> delete_card_from_local_button_ = nullptr;

  raw_ptr<LocalCardMigrationDialogView> parent_dialog_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MIGRATABLE_CARD_VIEW_H_
