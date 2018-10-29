// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_OFFER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_OFFER_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/autofill/view_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label_listener.h"

namespace autofill {

class LocalCardMigrationDialogController;

// A view composed of the main contents for local card migration dialog
// including title, explanatory message, migratable credit card list,
// horizontal separator, and legal message. It is used by
// LocalCardMigrationDialogView class when it offers the user the
// option to upload all browser-saved credit cards.
class LocalCardMigrationOfferView : public views::View,
                                    public views::StyledLabelListener {
 public:
  LocalCardMigrationOfferView(LocalCardMigrationDialogController* controller,
                              views::ButtonListener* listener);
  ~LocalCardMigrationOfferView() override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  const std::vector<std::string> GetSelectedCardGuids() const;

 private:
  void Init(views::ButtonListener* listener);

  LocalCardMigrationDialogController* controller_;

  views::View* card_list_view_ = nullptr;

  // The view that contains legal message and handles legal message links
  // clicking.
  LegalMessageView* legal_message_container_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationOfferView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_OFFER_VIEW_H_
