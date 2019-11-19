// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MIGRATABLE_CARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MIGRATABLE_CARD_VIEW_H_

#include "base/macros.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ButtonListener;
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
class MigratableCardView : public views::View, public views::ButtonListener {
 public:
  static const char kViewClassName[];

  MigratableCardView(const MigratableCreditCard& migratable_credit_card,
                     LocalCardMigrationDialogView* parent_dialog,
                     bool should_show_checkbox);
  ~MigratableCardView() override;

  bool IsSelected();
  std::string GetGuid();
  const base::string16 GetNetworkAndLastFourDigits() const;

  std::unique_ptr<views::View> GetMigratableCardDescriptionView(
      const MigratableCreditCard& migratable_credit_card,
      bool should_show_checkbox,
      ButtonListener* listener);

  // views::View
  const char* GetClassName() const override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  MigratableCreditCard migratable_credit_card_;

  // The checkbox_ can remain null if the card list in the local
  // card migration dialog contains only one card.
  views::Checkbox* checkbox_ = nullptr;

  views::View* checkbox_uncheck_text_container_ = nullptr;

  views::ImageButton* delete_card_from_local_button_ = nullptr;

  LocalCardMigrationDialogView* parent_dialog_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MigratableCardView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MIGRATABLE_CARD_VIEW_H_
