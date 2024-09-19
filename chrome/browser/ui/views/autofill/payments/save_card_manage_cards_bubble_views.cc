// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_manage_cards_bubble_views.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

SaveCardManageCardsBubbleViews::SaveCardManageCardsBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveCardBubbleController* controller)
    : SaveCardBubbleViews(anchor_view, web_contents, controller) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetExtraView(
      std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              [](SaveCardManageCardsBubbleViews* bubble) {
                bubble->controller()->OnManageCardsClicked();
                bubble->CloseBubble();
              },
              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_SAVED_PAYMENT_METHODS)))
      ->SetID(autofill::DialogViewId::MANAGE_CARDS_BUTTON);
}

std::unique_ptr<views::View>
SaveCardManageCardsBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> view =
      SaveCardBubbleViews::CreateMainContentView();
  view->SetID(DialogViewId::MANAGE_CARDS_VIEW);
  return view;
}

SaveCardManageCardsBubbleViews::~SaveCardManageCardsBubbleViews() = default;

BEGIN_METADATA(SaveCardManageCardsBubbleViews)
END_METADATA

}  // namespace autofill
