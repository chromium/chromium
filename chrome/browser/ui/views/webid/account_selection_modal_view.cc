// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"

#include <iostream>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

// The size of the spacing used between children elements.
constexpr int kBetweenChildSpacing = 4;
// The size of the horizontal padding for most elements in the dialog.
constexpr int kHorizontalPadding = 12;
// The size of the vertical padding for most elements in the dialog.
constexpr int kVerticalPadding = 8;
// The width of the modal dialog.
constexpr int kDialogWidth = 500;
// The margins of the modal dialog.
constexpr int kDialogMargin = 24;
// The size of brand icons of the modal dialog.
constexpr int kModalIconSize = 50;

AccountSelectionModalView::AccountSelectionModalView(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    content::WebContents* web_contents,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AccountSelectionViewBase::Observer* observer,
    views::WidgetObserver* widget_observer)
    : AccountSelectionViewBase(web_contents,
                               observer,
                               widget_observer,
                               std::move(url_loader_factory)) {
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetOwnedByWidget(true);
  set_margins(gfx::Insets::VH(kDialogMargin, kDialogMargin));
  set_fixed_width(kDialogWidth);
  SetShowTitle(false);
  SetShowCloseButton(false);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kBetweenChildSpacing));
  SetButtons(ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_CANCEL));

  title_ = GetTitle(top_frame_for_display, /*iframe_for_display=*/absl::nullopt,
                    idp_title, rp_context);
  SetAccessibleTitle(title_);
}

AccountSelectionModalView::~AccountSelectionModalView() {}

void AccountSelectionModalView::InitDialogWidget() {
  if (!web_contents_) {
    return;
  }

  views::Widget* widget =
      constrained_window::ShowWebModalDialogViews(this, web_contents_);
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(),
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
          ->delegate()
          ->GetWebContentsModalDialogHost());

  if (!widget) {
    return;
  }

  // Add the widget observer, if available. It is null in tests.
  if (widget_observer_) {
    widget->AddObserver(widget_observer_);
  }

  dialog_widget_ = widget->GetWeakPtr();
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateAccountChooserHeader(
    const content::IdentityProviderMetadata& idp_metadata) {
  std::unique_ptr<views::View> header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Add IDP icon, if available. Otherwise, fallback to the default globe icon.
  std::unique_ptr<BrandIconImageView> image_view =
      std::make_unique<BrandIconImageView>(
          base::BindOnce(&AccountSelectionViewBase::AddIdpImage,
                         weak_ptr_factory_.GetWeakPtr()),
          kModalIconSize);
  image_view->SetImageSize(gfx::Size(kModalIconSize, kModalIconSize));
  image_view->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_bottom(kVerticalPadding));
  if (idp_metadata.brand_icon_url.is_valid()) {
    ConfigureIdpBrandImageView(image_view.get(), idp_metadata);
  } else {
    image_view->SetImage(
        gfx::CreateVectorIcon(kGlobeIcon, kModalIconSize, gfx::kGoogleGrey700));
    image_view->SetVisible(true);
  }
  header->AddChildView(std::move(image_view));

  // Add the title.
  title_label_ = header->AddChildView(std::make_unique<views::Label>(
      title_, views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  SetLabelProperties(title_label_);

  // Add the body.
  views::Label* body_label =
      header->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_CHOOSE_AN_ACCOUNT),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_HINT));
  SetLabelProperties(body_label);
  return header;
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateMultipleAccountChooser(
    const std::vector<IdentityProviderDisplayData>& idp_display_data_list) {
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  views::View* const content =
      scroll_view->SetContents(std::make_unique<views::View>());
  content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  size_t num_rows = 0;
  for (const auto& idp_display_data : idp_display_data_list) {
    for (const auto& account : idp_display_data.accounts) {
      content->AddChildView(
          CreateAccountRow(account, idp_display_data, /*should_hover=*/true));
    }
    num_rows += idp_display_data.accounts.size();
  }

  const int per_account_size = content->GetPreferredSize().height() / num_rows;
  scroll_view->ClipHeightTo(0, static_cast<int>(per_account_size * 2.5f));
  return scroll_view;
}

void AccountSelectionModalView::ShowMultiAccountPicker(
    const std::vector<IdentityProviderDisplayData>& idp_display_data_list) {
  AddChildView(
      CreateAccountChooserHeader(idp_display_data_list[0].idp_metadata));
  AddChildView(CreateMultipleAccountChooser(idp_display_data_list));

  InitDialogWidget();

  // TODO(crbug.com/1518356): Connect with multi IDP API.
  // TODO(crbug.com/1518356): Connect with add account API.
  // TODO(crbug.com/1518356): Add permissions UI. This should include the
  // disclosure text.
}

void AccountSelectionModalView::ShowVerifyingSheet(
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    const std::u16string& title) {
  // TODO(crbug.com/1518356): Implement modal verifying sheet.
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateSingleAccountChooser(
    const IdentityProviderDisplayData& idp_display_data,
    const content::IdentityRequestAccount& account) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kHorizontalPadding), kVerticalPadding));
  // TODO(crbug.com/1518356): Currently, clicking on the account row triggers
  // the sign-in. To match the mocks, the account row should instead be selected
  // then clicking on a separate continue button triggers the sign-in. Also,
  // there should be an arrow to the right of the account.
  row->AddChildView(
      CreateAccountRow(account, idp_display_data, /*should_hover=*/true));
  return row;
}

void AccountSelectionModalView::ShowSingleAccountConfirmDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    bool show_back_button) {
  AddChildView(CreateAccountChooserHeader(idp_display_data.idp_metadata));
  AddChildView(CreateSingleAccountChooser(idp_display_data, account));

  InitDialogWidget();

  // TODO(crbug.com/1518356): Connect with multi IDP API.
  // TODO(crbug.com/1518356): Connect with add account API.
  // TODO(crbug.com/1518356): Add permissions UI. This should include the
  // disclosure text.
}

void AccountSelectionModalView::ShowFailureDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  // TODO(crbug.com/1518356): Implement modal failure dialog.
}

void AccountSelectionModalView::ShowErrorDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  // TODO(crbug.com/1518356): Implement modal error dialog.
}

void AccountSelectionModalView::CloseDialog() {
  if (!dialog_widget_) {
    return;
  }

  CancelDialog();
  // Remove the widget observer, if available. It is null in tests.
  if (widget_observer_) {
    dialog_widget_->RemoveObserver(widget_observer_);
  }
  dialog_widget_.reset();
}

std::string AccountSelectionModalView::GetDialogTitle() const {
  return base::UTF16ToUTF8(title_label_->GetText());
}

std::optional<std::string> AccountSelectionModalView::GetDialogSubtitle()
    const {
  // We do not support showing iframe domain at this point in time.
  return std::nullopt;
}

BEGIN_METADATA(AccountSelectionModalView)
END_METADATA
