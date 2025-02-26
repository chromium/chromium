// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_dialog.h"

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

using views::BoxLayout;
using views::BoxLayoutView;

namespace autofill {

BEGIN_METADATA(BnplTosDialog)
END_METADATA

BnplTosDialog::BnplTosDialog(
    base::WeakPtr<BnplTosController> controller,
    base::RepeatingCallback<void(const GURL&)> link_opener)
    : controller_(controller), link_opener_(link_opener) {
  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following two lines once this is the
  // default state for widgets and the delegates.
  SetOwnedByWidget(false);
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  ChromeLayoutProvider* chrome_layout_provider = ChromeLayoutProvider::Get();

  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(chrome_layout_provider->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_margins(chrome_layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
  SetShowCloseButton(false);
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kDefault);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_.get()->GetOkButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller_.get()->GetCancelButtonLabel());

  SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(),
      chrome_layout_provider->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  BoxLayoutView* content_view = AddChildView(std::make_unique<BoxLayoutView>());
  content_view->SetOrientation(BoxLayout::Orientation::kVertical);
  content_view->SetBetweenChildSpacing(
      chrome_layout_provider->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  content_view->AddChildView(CreateTextWithIconView(
      controller_.get()->GetReviewText(), /*text_link_info=*/std::nullopt,
      vector_icons::kChecklistIcon));

  content_view->AddChildView(CreateTextWithIconView(
      controller_.get()->GetApproveText(), /*text_link_info=*/std::nullopt,
      vector_icons::kReceiptLongIcon));

  TextWithLink link_text = controller_.get()->GetLinkText();
  TextLinkInfo link_info;
  link_info.offset = link_text.offset;
  link_info.callback = base::BindRepeating(link_opener_, link_text.url);
  content_view->AddChildView(CreateTextWithIconView(
      link_text.text, std::move(link_info), vector_icons::kAddLinkIcon));

  content_view->AddChildView(std::make_unique<views::Separator>())
      ->SetProperty(
          views::kMarginsKey,
          gfx::Insets().set_top(ChromeLayoutProvider::Get()->GetDistanceMetric(
              DISTANCE_CONTENT_LIST_VERTICAL_MULTI)));

  content_view->AddChildView(CreateLegalMessageView(
      controller_.get()->GetLegalMessageLines(),
      base::UTF8ToUTF16(controller_.get()->GetAccountInfo().email),
      GetProfileAvatar(controller_.get()->GetAccountInfo()), link_opener_));
}

BnplTosDialog::~BnplTosDialog() = default;

void BnplTosDialog::AddedToWidget() {
  // The view needs to be added to the widget before we can get the bubble frame
  // view.
  // TODO: crbug.com/391141123 - Choose icon based on BNPL issuer ID when the
  // controller is implemented.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          controller_.get()->GetTitle(),
          TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_AFFIRM));
}

BnplTosController* BnplTosDialog::controller() const {
  return controller_.get();
}

}  // namespace autofill
