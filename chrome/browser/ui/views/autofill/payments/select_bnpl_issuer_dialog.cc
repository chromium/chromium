// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/select_bnpl_issuer_dialog.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/autofill/payments/bnpl_dialog_footnote.h"
#include "chrome/browser/ui/views/autofill/payments/bnpl_issuer_view.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace payments {

namespace {

class SelectBnplIssuerViewDesktop : public SelectBnplIssuerView {
 public:
  SelectBnplIssuerViewDesktop(
      base::WeakPtr<SelectBnplIssuerDialogController> controller,
      content::WebContents* web_contents);
  SelectBnplIssuerViewDesktop(const SelectBnplIssuerViewDesktop&) = delete;
  SelectBnplIssuerViewDesktop& operator=(const SelectBnplIssuerViewDesktop&) =
      delete;
  ~SelectBnplIssuerViewDesktop() override;

  // SelectBnplIssuerView:
  void Dismiss() override;

 private:
  void CloseDialog(views::Widget::ClosedReason closed_reason);

  base::WeakPtr<SelectBnplIssuerDialogController> controller_;
  std::unique_ptr<views::Widget> dialog_;
};

SelectBnplIssuerViewDesktop::SelectBnplIssuerViewDesktop(
    base::WeakPtr<SelectBnplIssuerDialogController> controller,
    content::WebContents* web_contents)
    : controller_(controller) {
  auto* tab_interface = tabs::TabInterface::GetFromContents(web_contents);
  if (tab_interface) {
    auto select_bnpl_issuer_delegate =
        std::make_unique<SelectBnplIssuerDialog>(controller_, web_contents);
    dialog_ = tab_interface->GetTabFeatures()
                  ->tab_dialog_manager()
                  ->CreateShowDialogAndBlockTabInteraction(
                      select_bnpl_issuer_delegate.release());
    dialog_->MakeCloseSynchronous(base::BindOnce(
        &SelectBnplIssuerViewDesktop::CloseDialog, base::Unretained(this)));
  }
}

SelectBnplIssuerViewDesktop::~SelectBnplIssuerViewDesktop() = default;

void SelectBnplIssuerViewDesktop::Dismiss() {
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
  if (dialog_) {
    dialog_->CloseWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);
  }
}

void SelectBnplIssuerViewDesktop::CloseDialog(
    views::Widget::ClosedReason closed_reason) {
  if (closed_reason == views::Widget::ClosedReason::kCancelButtonClicked ||
      closed_reason == views::Widget::ClosedReason::kUnspecified) {
    if (controller_) {
      controller_->OnCancel();
      controller_ = nullptr;
    }
  }
  dialog_.reset();
}

}  // namespace

SelectBnplIssuerDialog::SelectBnplIssuerDialog(
    base::WeakPtr<SelectBnplIssuerDialogController> controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents->GetWeakPtr()) {
  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following two lines once this is the
  // default state for widgets and the delegates.
  views::WidgetDelegate::SetOwnedByWidget(false);
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  // TODO(crbug.com/363332740): Initialize the UI.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_CANCEL));
  SetShowCloseButton(false);
  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kText));
  SetTitle(controller_->GetTitle());
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::BoxLayout::Orientation::kVertical);

  container_view_ = AddChildView(std::make_unique<views::View>());
  container_view_->SetUseDefaultFillLayout(true);

  bnpl_issuer_view_ = container_view_->AddChildView(
      std::make_unique<BnplIssuerView>(controller_, this));

  TextWithLink link_text = controller_.get()->GetLinkText();
  TextLinkInfo link_info;
  link_info.offset = link_text.offset;
  link_info.callback =
      base::BindRepeating(&SelectBnplIssuerDialog::OnSettingsLinkClicked,
                          weak_ptr_factory_.GetWeakPtr());

  bnpl_footnote_view_ = SetFootnoteView(std::make_unique<BnplDialogFootnote>(
      link_text.text, std::move(link_info)));
}

SelectBnplIssuerDialog::~SelectBnplIssuerDialog() = default;

void SelectBnplIssuerDialog::DisplayThrobber() {
  bnpl_issuer_view_->SetVisible(false);
  views::Throbber* throbber = nullptr;
  container_view_->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .AddChild(views::Builder<views::Throbber>(
                        std::make_unique<views::Throbber>(24))
                        .CopyAddressTo(&throbber))
          .Build());
  throbber->Start();
  throbber->SizeToPreferredSize();
}

bool SelectBnplIssuerDialog::Accept() {
  // TODO(kylixrd): Should eventually return false and require the controller to
  // dismiss the dialog. This will eventually display a spinner.
  return views::DialogDelegate::Accept();
}

void SelectBnplIssuerDialog::AddedToWidget() {
  // The BubbleFrameView is only available after this view is added to the
  // Widget.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          // TODO(crbug.com/356443046): Move to resources and translate string.
          u"Choose a pay over time provider",
          TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
}

void SelectBnplIssuerDialog::OnSettingsLinkClicked() {
  if (!web_contents_) {
    return;
  }
  Browser* browser = chrome::FindBrowserWithTab(web_contents_.get());
  if (!browser) {
    return;
  }
  chrome::ShowSettingsSubPage(browser, chrome::kPaymentsSubPage);
}

BEGIN_METADATA(SelectBnplIssuerDialog)
END_METADATA

}  // namespace payments

std::unique_ptr<payments::SelectBnplIssuerView>
CreateAndShowBnplIssuerSelectionDialog(
    base::WeakPtr<payments::SelectBnplIssuerDialogController> controller,
    content::WebContents* web_contents) {
  auto select_issuer_view =
      std::make_unique<payments::SelectBnplIssuerViewDesktop>(controller,
                                                              web_contents);
  return select_issuer_view;
}

}  // namespace autofill
