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
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

// The size of the spacing used between children elements.
constexpr int kBetweenChildSpacing = 4;
// The size of the vertical padding for most elements in the dialog.
constexpr int kVerticalPadding = 8;
// The width of the modal dialog.
constexpr int kDialogWidth = 500;
// The margins of the modal dialog.
constexpr int kDialogMargin = 24;
// The size of brand icons of the modal dialog.
constexpr int kModalIconSize = 50;
// The height of the progress bar on the modal dialog.
constexpr int kModalProgressBarHeight = 4;

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
  set_fixed_width(kDialogWidth);
  SetShowTitle(false);
  SetShowCloseButton(false);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kBetweenChildSpacing));
  SetButtons(ui::DIALOG_BUTTON_NONE);

  title_ = GetTitle(top_frame_for_display, /*iframe_for_display=*/std::nullopt,
                    idp_title, rp_context);
  SetAccessibleTitle(title_);

  // TODO(crbug.com/1518356): Add loading modal UI.
}

AccountSelectionModalView::~AccountSelectionModalView() = default;

void AccountSelectionModalView::AddProgressBar() {
  // Change top margin of header to accommodate progress bar.
  CHECK(header_view_);
  constexpr int kVerifyingTopMargin = 16;
  static_cast<views::BoxLayout*>(header_view_->GetLayoutManager())
      ->set_inside_border_insets(gfx::Insets::TLBR(
          /*top=*/kVerifyingTopMargin, /*left=*/kDialogMargin, /*bottom=*/0,
          /*right=*/kDialogMargin));

  // Add progress bar.
  views::ProgressBar* progress_bar =
      AddChildViewAt(std::make_unique<views::ProgressBar>(), 0);
  progress_bar->SetPreferredHeight(kModalProgressBarHeight);
  progress_bar->SetPreferredCornerRadii(std::nullopt);

  // Use an infinite animation: SetValue(-1).
  progress_bar->SetValue(-1);
  progress_bar->SetBackgroundColor(SK_ColorLTGRAY);
  progress_bar->SetPreferredSize(
      gfx::Size(kDialogWidth, kModalProgressBarHeight));
  progress_bar->SizeToPreferredSize();
}

void AccountSelectionModalView::UpdateModalPositionAndTitle() {
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(),
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
          ->delegate()
          ->GetWebContentsModalDialogHost());
  GetWidget()->UpdateWindowTitle();
}

void AccountSelectionModalView::InitDialogWidget() {
  if (!web_contents_) {
    return;
  }

  if (dialog_widget_) {
    UpdateModalPositionAndTitle();
    return;
  }

  views::Widget* widget =
      constrained_window::ShowWebModalDialogViews(this, web_contents_);
  if (!widget) {
    return;
  }
  UpdateModalPositionAndTitle();

  // Add the widget observer, if available. It is null in tests.
  if (widget_observer_) {
    widget->AddObserver(widget_observer_);
  }

  dialog_widget_ = widget->GetWeakPtr();
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreatePlaceholderAccountRow() {
  std::unique_ptr<views::View> placeholder_account_icon =
      std::make_unique<views::View>();
  placeholder_account_icon->SetPreferredSize(
      gfx::Size(kDesiredAvatarSize, kDesiredAvatarSize));
  placeholder_account_icon->SizeToPreferredSize();
  placeholder_account_icon->SetBackground(
      views::CreateRoundedRectBackground(SK_ColorLTGRAY, kDesiredAvatarSize));

  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(/*vertical=*/kVerticalSpacing,
                      /*horizontal=*/kDialogMargin),
      kLeftRightPadding));
  row->AddChildView(std::move(placeholder_account_icon));

  constexpr int kPlaceholderVerticalSpacing = 2;
  views::View* const text_column =
      row->AddChildView(std::make_unique<views::View>());
  text_column->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(/*vertical=*/kPlaceholderVerticalSpacing,
                                  /*horizontal=*/0));

  constexpr int kPlaceholderRadius = 2;
  constexpr int kPlaceholderTextHeight = 10;
  constexpr int kPlaceholderAccountNameWidth = 80;
  constexpr int kPlaceholderAccountEmailWidth = 130;
  views::View* placeholder_account_name =
      text_column->AddChildView(std::make_unique<views::View>());
  placeholder_account_name->SetPreferredSize(
      gfx::Size(kPlaceholderAccountNameWidth, kPlaceholderTextHeight));
  placeholder_account_name->SizeToPreferredSize();
  placeholder_account_name->SetBackground(
      views::CreateRoundedRectBackground(SK_ColorLTGRAY, kPlaceholderRadius));

  views::View* placeholder_account_email =
      text_column->AddChildView(std::make_unique<views::View>());
  placeholder_account_email->SetPreferredSize(
      gfx::Size(kPlaceholderAccountEmailWidth, kPlaceholderTextHeight));
  placeholder_account_email->SizeToPreferredSize();
  placeholder_account_email->SetBackground(
      views::CreateRoundedRectBackground(SK_ColorLTGRAY, kPlaceholderRadius));

  return row;
}

std::unique_ptr<views::View> AccountSelectionModalView::CreateButtonRow(
    std::optional<views::Button::PressedCallback> continue_callback,
    std::optional<views::Button::PressedCallback> use_other_account_callback) {
  const views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  std::unique_ptr<views::View> button_container =
      std::make_unique<views::View>();
  button_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(/*vertical=*/0,
                          /*horizontal=*/layout_provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_BUTTON_HORIZONTAL)))
      .SetInteriorMargin(gfx::Insets::TLBR(/*top=*/0, /*left=*/kDialogMargin,
                                           /*bottom=*/kDialogMargin,
                                           /*right=*/kDialogMargin));

  if (use_other_account_callback) {
    auto use_other_account_button_container =
        std::make_unique<views::FlexLayoutView>();
    use_other_account_button_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded));
    std::unique_ptr<views::MdTextButton> use_other_account_button =
        std::make_unique<views::MdTextButton>(
            std::move(*use_other_account_callback),
            l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_USE_OTHER_ACCOUNT));
    use_other_account_button->SetStyle(ui::ButtonStyle::kDefault);
    use_other_account_button->SetAppearDisabledInInactiveWidget(true);
    use_other_account_button_container->AddChildView(
        std::move(use_other_account_button));
    button_container->AddChildView(
        std::move(use_other_account_button_container));
  }

  std::unique_ptr<views::MdTextButton> cancel_button =
      std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &AccountSelectionViewBase::Observer::OnCloseButtonClicked,
              base::Unretained(observer_)),
          l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button_ = cancel_button.get();
  cancel_button->SetStyle(ui::ButtonStyle::kDefault);
  cancel_button->SetAppearDisabledInInactiveWidget(true);
  button_container->AddChildView(std::move(cancel_button));

  if (continue_callback) {
    std::unique_ptr<views::MdTextButton> continue_button =
        std::make_unique<views::MdTextButton>(
            std::move(*continue_callback),
            l10n_util::GetStringUTF16(IDS_SIGNIN_CONTINUE));
    continue_button->SetStyle(ui::ButtonStyle::kProminent);
    continue_button->SetAppearDisabledInInactiveWidget(true);
    button_container->AddChildView(std::move(continue_button));
  }

  // TODO(crbug.com/1518356): Add back button.

  return button_container;
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateAccountChooserHeader(
    const content::IdentityProviderMetadata& idp_metadata =
        content::IdentityProviderMetadata()) {
  std::unique_ptr<views::View> header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(/*top=*/kDialogMargin, /*left=*/kDialogMargin,
                        /*bottom=*/0,
                        /*right=*/kDialogMargin)));

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
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(/*vertical=*/0, /*horizontal=*/kDialogMargin)));
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
  RemoveAllChildViews();

  header_view_ = AddChildView(
      CreateAccountChooserHeader(idp_display_data_list[0].idp_metadata));
  account_chooser_ =
      AddChildView(CreateMultipleAccountChooser(idp_display_data_list));

  std::optional<views::Button::PressedCallback> use_other_account_callback =
      std::nullopt;

  // TODO(crbug.com/324052630): Support add account with multi IDP API.
  if (idp_display_data_list[0].idp_metadata.supports_add_account) {
    use_other_account_callback = base::BindRepeating(
        &AccountSelectionViewBase::Observer::OnLoginToIdP,
        base::Unretained(observer_),
        idp_display_data_list[0].idp_metadata.config_url,
        idp_display_data_list[0].idp_metadata.idp_login_url);
  }
  button_row_ =
      AddChildView(CreateButtonRow(/*continue_callback=*/std::nullopt,
                                   std::move(use_other_account_callback)));

  InitDialogWidget();

  // TODO(crbug.com/324052630): Connect with multi IDP API.
}

void AccountSelectionModalView::ShowVerifyingSheet(
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    const std::u16string& title) {
  // A different type of sheet must have been shown prior to ShowVerifyingSheet.
  // This might change if we choose to integrate auto re-authn with button mode.
  CHECK(dialog_widget_);

  AddProgressBar();

  // Disable account chooser.
  CHECK(account_chooser_);
  for (const auto& account_row : account_chooser_->children()) {
    account_row->SetEnabled(false);
  }

  // Disable buttons.
  CHECK(button_row_);
  for (const auto& button : button_row_->children()) {
    if (button != cancel_button_) {
      button->SetEnabled(false);
    }
  }

  InitDialogWidget();
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateSingleAccountChooser(
    const IdentityProviderDisplayData& idp_display_data,
    const content::IdentityRequestAccount& account,
    bool should_hover,
    bool show_disclosure_label) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(/*vertical=*/0, /*horizontal=*/kDialogMargin),
      /*between_child_spacing=*/kVerticalPadding));
  // TODO(crbug.com/1518356): There should be an arrow to the right of the
  // account when the account row is hoverable.
  row->AddChildView(CreateAccountRow(account, idp_display_data, should_hover));
  if (show_disclosure_label) {
    row->AddChildView(CreateDisclosureLabel(idp_display_data));
  }
  return row;
}

void AccountSelectionModalView::ShowSingleAccountConfirmDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    bool show_back_button) {
  RemoveAllChildViews();

  header_view_ =
      AddChildView(CreateAccountChooserHeader(idp_display_data.idp_metadata));
  account_chooser_ =
      AddChildView(CreateSingleAccountChooser(idp_display_data, account,
                                              /*should_hover=*/true,
                                              /*show_disclosure_label=*/false));

  std::optional<views::Button::PressedCallback> use_other_account_callback =
      std::nullopt;
  if (idp_display_data.idp_metadata.supports_add_account) {
    use_other_account_callback = base::BindRepeating(
        &AccountSelectionViewBase::Observer::OnLoginToIdP,
        base::Unretained(observer_), idp_display_data.idp_metadata.config_url,
        idp_display_data.idp_metadata.idp_login_url);
  }
  button_row_ = AddChildView(CreateButtonRow(
      base::BindRepeating(
          &AccountSelectionViewBase::Observer::OnAccountSelected,
          base::Unretained(observer_), std::cref(account),
          std::cref(idp_display_data)),
      std::move(use_other_account_callback)));

  InitDialogWidget();

  // TODO(crbug.com/324052630): Connect with multi IDP API.
}

void AccountSelectionModalView::ShowFailureDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  NOTREACHED()
      << "ShowFailureDialog is only implemented for AccountSelectionBubbleView";
}

void AccountSelectionModalView::ShowErrorDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  NOTREACHED()
      << "ShowErrorDialog is only implemented for AccountSelectionBubbleView";
}

void AccountSelectionModalView::ShowLoadingDialog() {
  header_view_ = AddChildView(CreateAccountChooserHeader());
  AddProgressBar();
  AddChildView(CreatePlaceholderAccountRow());
  button_row_ = AddChildView(
      CreateButtonRow(/*continue_callback=*/std::nullopt,
                      /*use_other_account_callback=*/std::nullopt));

  InitDialogWidget();
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateRequestPermissionHeader(
    const content::IdentityProviderMetadata& idp_metadata) {
  std::unique_ptr<views::View> header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(/*top=*/kDialogMargin, /*left=*/kDialogMargin,
                        /*bottom=*/0,
                        /*right=*/kDialogMargin)));

  // TODO(crbug.com/1518356): Show RP icon instead of IDP icon.
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

  return header;
}

void AccountSelectionModalView::ShowRequestPermissionDialog(
    const std::u16string& top_frame_for_display,
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data) {
  RemoveAllChildViews();
  title_ = l10n_util::GetStringFUTF16(IDS_ACCOUNT_SELECTION_CONFIRM_ACCOUNT,
                                      top_frame_for_display,
                                      idp_display_data.idp_etld_plus_one);
  SetAccessibleTitle(title_);
  header_view_ = AddChildView(
      CreateRequestPermissionHeader(idp_display_data.idp_metadata));
  account_chooser_ =
      AddChildView(CreateSingleAccountChooser(idp_display_data, account,
                                              /*should_hover=*/false,
                                              /*show_disclosure_label=*/true));
  button_row_ = AddChildView(CreateButtonRow(
      base::BindRepeating(
          &AccountSelectionViewBase::Observer::OnAccountSelected,
          base::Unretained(observer_), std::cref(account),
          std::cref(idp_display_data)),
      /*use_other_account_callback=*/std::nullopt));

  InitDialogWidget();
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
