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
#include "chrome/browser/ui/views/autofill/payments/bnpl_dialog_footnote.h"
#include "chrome/browser/ui/views/autofill/payments/bnpl_issuer_view.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace payments {

namespace {

class SelectBnplIssuerViewDesktop : public SelectBnplIssuerView {
 public:
  SelectBnplIssuerViewDesktop(
      base::WeakPtr<SelectBnplIssuerDialogController> controller,
      content::WebContents* web_contents,
      bool has_seen_ai_terms);
  SelectBnplIssuerViewDesktop(const SelectBnplIssuerViewDesktop&) = delete;
  SelectBnplIssuerViewDesktop& operator=(const SelectBnplIssuerViewDesktop&) =
      delete;
  ~SelectBnplIssuerViewDesktop() override;

  void UpdateDialogWithIssuers() override;

 private:
  base::WeakPtr<SelectBnplIssuerDialogController> controller_;
  std::unique_ptr<views::Widget> dialog_;
  base::WeakPtr<SelectBnplIssuerDialog> select_bnpl_issuer_dialog_;
};

SelectBnplIssuerViewDesktop::SelectBnplIssuerViewDesktop(
    base::WeakPtr<SelectBnplIssuerDialogController> controller,
    content::WebContents* web_contents,
    bool has_seen_ai_terms)
    : controller_(controller) {
  auto* tab_interface = tabs::TabInterface::GetFromContents(web_contents);
  if (tab_interface) {
    auto select_bnpl_issuer_dialog_delegate =
        std::make_unique<SelectBnplIssuerDialog>(controller_, web_contents,
                                                 has_seen_ai_terms);
    select_bnpl_issuer_dialog_ =
        select_bnpl_issuer_dialog_delegate->GetWeakPtr();

    dialog_ = tab_interface->GetTabFeatures()
                  ->tab_dialog_manager()
                  ->CreateAndShowDialog(
                      select_bnpl_issuer_dialog_delegate.release(),
                      std::make_unique<tabs::TabDialogManager::Params>());
  }
}

SelectBnplIssuerViewDesktop::~SelectBnplIssuerViewDesktop() = default;

void SelectBnplIssuerViewDesktop::UpdateDialogWithIssuers() {
  if (dialog_ && select_bnpl_issuer_dialog_) {
    select_bnpl_issuer_dialog_->DismissThrobberAndShowIssuerView();
  }
}

}  // namespace

BEGIN_METADATA(SelectBnplIssuerDialog)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SelectBnplIssuerDialog, kThrobberId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SelectBnplIssuerDialog, kBnplIssuerView);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SelectBnplIssuerDialog, kFootnoteViewId);

SelectBnplIssuerDialog::SelectBnplIssuerDialog(
    base::WeakPtr<SelectBnplIssuerDialogController> controller,
    content::WebContents* web_contents,
    bool has_seen_ai_terms)
    : controller_(controller), web_contents_(web_contents->GetWeakPtr()) {
  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following line once this is the
  // default state for widgets.
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
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::BoxLayout::Orientation::kVertical);
  // This sets the cancel callback in DialogDelegate, which this class extends,
  // and the callback is not run during destruction. Thus, `this` will always be
  // present when it needs to be run.
  SetCancelCallbackWithClose(base::BindRepeating(
      &SelectBnplIssuerDialog::OnCancelled, base::Unretained(this)));

  container_view_ = AddChildView(std::make_unique<views::View>());
  container_view_->SetUseDefaultFillLayout(true);

  CreateThrobberView();
  CreateBnplIssuerView();
  if (base::FeatureList::IsEnabled(
          ::autofill::features::kAutofillEnableAiBasedAmountExtraction) &&
      has_seen_ai_terms) {
    throbber_container_view_->SetVisible(true);
    bnpl_issuer_view_->SetVisible(false);
  } else {
    throbber_container_view_->SetVisible(false);
    bnpl_issuer_view_->SetVisible(true);
  }

  TextWithLink link_text = controller_.get()->GetLinkText();
  TextLinkInfo link_info;
  link_info.bold_range = link_text.bold_range;
  link_info.offset = link_text.offset;
  link_info.callback =
      base::BindRepeating(&SelectBnplIssuerDialog::OnSettingsLinkClicked,
                          weak_ptr_factory_.GetWeakPtr());
  bnpl_footnote_view_ = SetFootnoteView(std::make_unique<BnplDialogFootnote>(
      link_text.text, std::move(link_info)));
  bnpl_footnote_view_->SetProperty(views::kElementIdentifierKey,
                                   SelectBnplIssuerDialog::kFootnoteViewId);
}

SelectBnplIssuerDialog::~SelectBnplIssuerDialog() = default;

void SelectBnplIssuerDialog::DisplayThrobber() {
  bnpl_issuer_view_->SetVisible(false);
  throbber_container_view_->SetVisible(true);
}

void SelectBnplIssuerDialog::DismissThrobberAndShowIssuerView() {
  throbber_->Stop();
  throbber_container_view_->SetVisible(false);
  bnpl_issuer_view_->UpdateIssuers();
  bnpl_issuer_view_->SetVisible(true);
}

bool SelectBnplIssuerDialog::OnCancelled() {
  // Deletes `this`. Do not access any class variables or `this` in any way
  // after `controller_->OnUserCancelled()`.
  controller_->OnUserCancelled();
  return false;
}

void SelectBnplIssuerDialog::AddedToWidget() {
  std::u16string title = controller_->GetTitle();
  // The BubbleFrameView is only available after this view is added to the
  // Widget.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          title, TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);
  SetAccessibleTitle(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_BNPL_SELECT_PROVIDER_TITLE_DESCRIPTION, title,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME)));
}

void SelectBnplIssuerDialog::CreateThrobberView() {
  if (!throbber_container_view_) {
    throbber_container_view_ = container_view_->AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetCrossAxisAlignment(
                views::BoxLayout::CrossAxisAlignment::kCenter)
            .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
            .SetProperty(views::kElementIdentifierKey,
                         SelectBnplIssuerDialog::kThrobberId)
            .Build());
    throbber_ = throbber_container_view_->AddChildView(
        std::make_unique<views::Throbber>(24));
    throbber_->SizeToPreferredSize();
  }
  throbber_->Start();
  throbber_->GetViewAccessibility().AnnouncePolitely(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_BNPL_PROGRESS_DIALOG_LOADING_MESSAGE));
}

void SelectBnplIssuerDialog::CreateBnplIssuerView() {
  bnpl_issuer_view_ = container_view_->AddChildView(
      std::make_unique<BnplIssuerView>(controller_, this));
  bnpl_issuer_view_->SetProperty(views::kElementIdentifierKey,
                                 SelectBnplIssuerDialog::kBnplIssuerView);
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

}  // namespace payments

std::unique_ptr<payments::SelectBnplIssuerView>
CreateAndShowBnplIssuerSelectionDialog(
    base::WeakPtr<payments::SelectBnplIssuerDialogController> controller,
    content::WebContents* web_contents,
    bool has_seen_ai_terms) {
  auto select_issuer_view =
      std::make_unique<payments::SelectBnplIssuerViewDesktop>(
          controller, web_contents, has_seen_ai_terms);
  return select_issuer_view;
}

}  // namespace autofill
