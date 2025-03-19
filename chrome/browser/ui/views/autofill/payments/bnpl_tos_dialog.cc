// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_dialog.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

using views::BoxLayout;
using views::BoxLayoutView;

namespace autofill {

BEGIN_METADATA(BnplTosDialog)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BnplTosDialog, kThrobberId);

BnplTosDialog::BnplTosDialog(
    base::WeakPtr<BnplTosController> controller,
    base::RepeatingCallback<void(const GURL&)> link_opener)
    : controller_(controller), link_opener_(link_opener) {
  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following two lines once this is the
  // default state for widgets and the delegates.
  SetOwnedByWidget(/*delete_self=*/false);
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetAcceptCallbackWithClose(
      base::BindRepeating(&BnplTosDialog::OnAccepted, base::Unretained(this)));
  SetCancelCallbackWithClose(
      base::BindRepeating(&BnplTosDialog::OnCancelled, base::Unretained(this)));

  ChromeLayoutProvider* chrome_layout_provider = ChromeLayoutProvider::Get();

  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(chrome_layout_provider->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_margins(chrome_layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
  SetShowCloseButton(false);
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kDefault);
  SetButtonLabel(ui::mojom::DialogButton::kOk, controller_->GetOkButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller_->GetCancelButtonLabel());
  SetLayoutManager(std::make_unique<BoxLayout>());

  container_view_ = AddChildView(std::make_unique<views::View>());
  container_view_->SetUseDefaultFillLayout(true);

  content_view_ =
      container_view_->AddChildView(std::make_unique<BoxLayoutView>());
  content_view_->SetOrientation(BoxLayout::Orientation::kVertical);
  content_view_->SetBetweenChildSpacing(
      chrome_layout_provider->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  content_view_->AddChildView(CreateTextWithIconView(
      controller_->GetReviewText(), /*text_link_info=*/std::nullopt,
      vector_icons::kChecklistIcon));

  content_view_->AddChildView(CreateTextWithIconView(
      controller_->GetApproveText(), /*text_link_info=*/std::nullopt,
      vector_icons::kReceiptLongIcon));

  TextWithLink link_text = controller_->GetLinkText();
  TextLinkInfo link_info;
  link_info.offset = link_text.offset;
  link_info.callback = base::BindRepeating(link_opener_, link_text.url);
  content_view_->AddChildView(CreateTextWithIconView(
      link_text.text, std::move(link_info), vector_icons::kAddLinkIcon));

  content_view_->AddChildView(std::make_unique<views::Separator>())
      ->SetProperty(
          views::kMarginsKey,
          gfx::Insets().set_top(ChromeLayoutProvider::Get()->GetDistanceMetric(
              DISTANCE_CONTENT_LIST_VERTICAL_MULTI)));

  content_view_->AddChildView(CreateLegalMessageView(
      controller_->GetLegalMessageLines(),
      base::UTF8ToUTF16(controller_->GetAccountInfo().email),
      GetProfileAvatar(controller_->GetAccountInfo()), link_opener_));

  throbber_view_ =
      container_view_->AddChildView(std::make_unique<BoxLayoutView>());
  throbber_view_->SetVisible(false);
  throbber_view_->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  throbber_view_->SetCrossAxisAlignment(BoxLayout::CrossAxisAlignment::kCenter);
  throbber_ = throbber_view_->AddChildView(
      std::make_unique<views::Throbber>(kDialogThrobberDiameter));
  throbber_->SetProperty(views::kElementIdentifierKey,
                         BnplTosDialog::kThrobberId);
}

BnplTosDialog::~BnplTosDialog() = default;

void BnplTosDialog::AddedToWidget() {
  // The view needs to be added to the widget before we can get the bubble frame
  // view.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(controller_->GetTitle(),
                                                    GetTitleIcon()));
}

TitleWithIconAfterLabelView::Icon BnplTosDialog::GetTitleIcon() const {
  const std::string& issuer_id = controller_->GetIssuerId();

  if (issuer_id == kBnplAffirmIssuerId) {
    return TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_AFFIRM;
  }
  if (issuer_id == kBnplZipIssuerId) {
    return TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_ZIP;
  }

  // TODO: crbug.com/401282730 - Return Google Pay Icon as a graceful failure
  // case until the BNPL issuer ID is converted into an enum.
  return TitleWithIconAfterLabelView::Icon::GOOGLE_PAY;
}

bool BnplTosDialog::OnAccepted() {
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);

  throbber_->SizeToPreferredSize();
  throbber_->Start();
  content_view_->SetVisible(false);
  throbber_view_->SetVisible(true);

  // This call will destroy `this` and no members should be referenced
  // afterwards.
  controller_->OnUserAccepted();

  return false;
}

bool BnplTosDialog::OnCancelled() {
  // This call will destroy `this` and no members should be referenced
  // afterwards.
  controller_->OnUserCancelled();

  return false;
}

}  // namespace autofill
