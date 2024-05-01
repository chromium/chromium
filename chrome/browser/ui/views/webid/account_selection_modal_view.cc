// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"

#include <iostream>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/browser/ui/views/webid/webid_utils.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
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
constexpr int kDialogWidth = 448;
// The margins of the modal dialog.
constexpr int kDialogMargin = 20;

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

  title_ = webid::GetTitle(top_frame_for_display,
                           /*iframe_for_display=*/std::nullopt, idp_title,
                           rp_context);
}

AccountSelectionModalView::~AccountSelectionModalView() = default;

void AccountSelectionModalView::AddProgressBar() {
  // Change top margin of header to accommodate progress bar.
  CHECK(header_view_);
  constexpr int kVerifyingTopMargin = 13;
  static_cast<views::BoxLayout*>(header_view_->GetLayoutManager())
      ->set_inside_border_insets(gfx::Insets::TLBR(
          /*top=*/kVerifyingTopMargin, /*left=*/kDialogMargin,
          /*bottom=*/kVerticalPadding,
          /*right=*/kDialogMargin));

  // Add progress bar.
  constexpr int kModalProgressBarHeight = 3;
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

  has_progress_bar_ = true;
}

void AccountSelectionModalView::UpdateModalPositionAndTitle() {
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(),
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
          ->delegate()
          ->GetWebContentsModalDialogHost());

  if (accessibility_state_utils::IsScreenReaderEnabled()) {
    GetInitiallyFocusedView()->RequestFocus();
  }
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
      gfx::Size(kModalAvatarSize, kModalAvatarSize));
  placeholder_account_icon->SizeToPreferredSize();
  placeholder_account_icon->SetBackground(views::CreateRoundedRectBackground(
      gfx::kGoogleGrey200, kModalAvatarSize));

  constexpr int kPlaceholderAccountRowPadding = 16;
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(/*vertical=*/kPlaceholderAccountRowPadding,
                      /*horizontal=*/kDialogMargin + kModalHorizontalSpacing),
      /*between_child_spacing=*/kModalHorizontalSpacing));
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

  constexpr int kPlaceholderRadius = 5;
  constexpr int kPlaceholderTextHeight = 10;
  constexpr int kPlaceholderAccountNameWidth = 79;
  constexpr int kPlaceholderAccountEmailWidth = 134;
  views::View* placeholder_account_name =
      text_column->AddChildView(std::make_unique<views::View>());
  placeholder_account_name->SetPreferredSize(
      gfx::Size(kPlaceholderAccountNameWidth, kPlaceholderTextHeight));
  placeholder_account_name->SizeToPreferredSize();
  placeholder_account_name->SetBackground(views::CreateRoundedRectBackground(
      gfx::kGoogleGrey200, kPlaceholderRadius));

  views::View* placeholder_account_email =
      text_column->AddChildView(std::make_unique<views::View>());
  placeholder_account_email->SetPreferredSize(
      gfx::Size(kPlaceholderAccountEmailWidth, kPlaceholderTextHeight));
  placeholder_account_email->SizeToPreferredSize();
  placeholder_account_email->SetBackground(views::CreateRoundedRectBackground(
      gfx::kGoogleGrey200, kPlaceholderRadius));

  return row;
}

std::unique_ptr<views::View> AccountSelectionModalView::CreateButtonRow(
    std::optional<views::Button::PressedCallback> continue_callback =
        std::nullopt,
    std::optional<views::Button::PressedCallback> use_other_account_callback =
        std::nullopt,
    std::optional<views::Button::PressedCallback> back_callback =
        std::nullopt) {
  const views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  std::unique_ptr<views::View> button_container =
      std::make_unique<views::View>();
  constexpr int kButtonRowTopPadding = 24;
  button_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(
                      /*top=*/0, /*left=*/
                                 layout_provider->GetDistanceMetric(
                                     views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                      /*bottom=*/0, /*right=*/0))
      .SetInteriorMargin(gfx::Insets::TLBR(/*top=*/kButtonRowTopPadding,
                                           /*left=*/kDialogMargin,
                                           /*bottom=*/kDialogMargin,
                                           /*right=*/kDialogMargin));

  std::unique_ptr<views::MdTextButton> cancel_button =
      std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &AccountSelectionViewBase::Observer::OnCloseButtonClicked,
              base::Unretained(observer_)),
          l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button_ = cancel_button.get();
  // When a continue button is present, the cancel button should be more
  // prominent (Tonal) to align with common practice.
  cancel_button->SetStyle(continue_callback ? ui::ButtonStyle::kTonal
                                            : ui::ButtonStyle::kDefault);
  cancel_button->SetAppearDisabledInInactiveWidget(true);
  button_container->AddChildView(std::move(cancel_button));

  if (continue_callback) {
    std::unique_ptr<views::MdTextButton> continue_button =
        std::make_unique<views::MdTextButton>(
            std::move(*continue_callback),
            l10n_util::GetStringUTF16(IDS_SIGNIN_CONTINUE));
    continue_button_ = continue_button.get();
    continue_button->SetStyle(ui::ButtonStyle::kProminent);
    continue_button->SetAppearDisabledInInactiveWidget(true);
    button_container->AddChildView(std::move(continue_button));
  }

  if (!(use_other_account_callback || back_callback)) {
    return button_container;
  }

  CHECK(!use_other_account_callback || !back_callback);

  // Use other account or back button shown on the far left needs to be in its
  // own child container because we want it aligned to the start of the button
  // row container, whereas the other buttons are aligned to the end of the
  // button row container.
  std::unique_ptr<views::FlexLayoutView> leftmost_button_container =
      std::make_unique<views::FlexLayoutView>();
  leftmost_button_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  if (use_other_account_callback) {
    std::unique_ptr<views::MdTextButton> use_other_account_button =
        std::make_unique<views::MdTextButton>(
            std::move(*use_other_account_callback),
            l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_USE_OTHER_ACCOUNT));
    use_other_account_button_ = use_other_account_button.get();
    use_other_account_button->SetStyle(ui::ButtonStyle::kDefault);
    use_other_account_button->SetAppearDisabledInInactiveWidget(true);
    leftmost_button_container->AddChildView(
        std::move(use_other_account_button));
  } else {
    CHECK(back_callback);
    std::unique_ptr<views::MdTextButton> back_button =
        std::make_unique<views::MdTextButton>(
            std::move(*back_callback),
            l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_BACK));
    back_button_ = back_button.get();
    back_button->SetStyle(ui::ButtonStyle::kDefault);
    back_button->SetAppearDisabledInInactiveWidget(true);
    leftmost_button_container->AddChildView(std::move(back_button));
  }
  button_container->AddChildViewAt(std::move(leftmost_button_container), 0);

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
                        /*bottom=*/kVerticalPadding, /*right=*/kDialogMargin),
      /*between_child_spacing=*/kVerticalPadding));

  // Add IDP icon, if available. Otherwise, fallback to the default globe icon.
  header->AddChildView(CreateBrandIconImageView(idp_metadata.brand_icon_url));

  // Add the title.
  title_label_ = header->AddChildView(
      std::make_unique<views::Label>(title_, views::style::CONTEXT_DIALOG_TITLE,
                                     views::style::STYLE_HEADLINE_4));
  SetLabelProperties(title_label_);
  title_label_->SetFocusBehavior(FocusBehavior::ALWAYS);

  // Add the body.
  body_label_ = header->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_CHOOSE_AN_ACCOUNT),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_4));
  SetLabelProperties(body_label_);
  body_label_->SetFocusBehavior(FocusBehavior::ALWAYS);
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
      gfx::Insets::VH(/*vertical=*/kVerticalPadding,
                      /*horizontal=*/kDialogMargin)));

  // Add separator before the account rows.
  content->AddChildView(std::make_unique<views::Separator>());

  size_t num_rows = 0;
  constexpr int kMultipleAccountsVerticalPadding = 2;
  for (const auto& idp_display_data : idp_display_data_list) {
    for (const auto& account : idp_display_data.accounts) {
      content->AddChildView(CreateAccountRow(
          account, idp_display_data,
          /*should_hover=*/true,
          /*should_include_idp=*/false,
          /*is_modal_dialog=*/true,
          /*additional_vertical_padding=*/kMultipleAccountsVerticalPadding));
      // Add separator after each account row.
      content->AddChildView(std::make_unique<views::Separator>());
    }
    num_rows += idp_display_data.accounts.size();
  }

  const int per_account_size = content->GetPreferredSize().height() / num_rows;
  scroll_view->ClipHeightTo(0, static_cast<int>(per_account_size * 3.5f));
  return scroll_view;
}

void AccountSelectionModalView::ShowMultiAccountPicker(
    const std::vector<IdentityProviderDisplayData>& idp_display_data_list,
    bool show_back_button) {
  DCHECK(!show_back_button);
  RemoveNonHeaderChildViews();

  ConfigureBrandImageView(brand_icon_,
                          idp_display_data_list[0].idp_metadata.brand_icon_url);

  // Show the "Choose an account to continue" label.
  CHECK(body_label_);
  body_label_->SetVisible(/*visible=*/true);

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

  queued_announcement_ = l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE);

  // When a user signs in to the IdP with a returning account while the loading
  // modal is shown, we can exit without updating the UI.
  if (account.browser_trusted_login_state != Account::LoginState::kSignUp &&
      has_progress_bar_) {
    InitDialogWidget();
    return;
  }

  AddProgressBar();

  // Disable account chooser.
  CHECK(account_chooser_);
  for (const auto& account_row : account_chooser_->children()) {
    account_row->SetEnabled(false);
  }

  // Disable text buttons.
  if (use_other_account_button_) {
    use_other_account_button_->SetEnabled(false);
  }

  if (back_button_) {
    back_button_->SetEnabled(false);
  }

  if (continue_button_) {
    continue_button_->SetEnabled(false);
  }

  InitDialogWidget();
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateSingleAccountChooser(
    const IdentityProviderDisplayData& idp_display_data,
    const content::IdentityRequestAccount& account,
    bool should_hover,
    bool show_disclosure_label,
    bool show_separator,
    int additional_row_vertical_padding) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(/*vertical=*/0, /*horizontal=*/kDialogMargin)));

  // Add separator before the account row.
  if (show_separator) {
    row->AddChildView(std::make_unique<views::Separator>());
  }

  // Add account row.
  row->AddChildView(CreateAccountRow(account, idp_display_data, should_hover,
                                     /*should_include_idp=*/false,
                                     /*is_modal_dialog=*/true,
                                     additional_row_vertical_padding));

  // Add separator after the account row.
  if (show_separator) {
    row->AddChildView(std::make_unique<views::Separator>());
  }

  // Add disclosure label.
  if (show_disclosure_label) {
    std::unique_ptr<views::StyledLabel> disclosure_label =
        CreateDisclosureLabel(idp_display_data);
    disclosure_label->SetDefaultTextStyle(views::style::STYLE_BODY_4);
    disclosure_label->SizeToFit(views::LayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
    disclosure_label->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        /*top=*/kVerticalSpacing, /*left=*/0, /*bottom=*/0, /*right=*/0)));
    queued_announcement_ = disclosure_label->GetText();
    row->AddChildView(std::move(disclosure_label));
  }
  return row;
}

void AccountSelectionModalView::ShowSingleAccountConfirmDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    bool show_back_button) {
  RemoveNonHeaderChildViews();

  ConfigureBrandImageView(brand_icon_,
                          idp_display_data.idp_metadata.brand_icon_url);

  // Show the "Choose an account to continue" label.
  CHECK(body_label_);
  body_label_->SetVisible(/*visible=*/true);

  account_chooser_ = AddChildView(CreateSingleAccountChooser(
      idp_display_data, account,
      /*should_hover=*/true,
      /*show_disclosure_label=*/false,
      /*show_separator=*/true,
      /*additional_row_vertical_padding=*/kVerticalPadding));

  std::optional<views::Button::PressedCallback> use_other_account_callback =
      std::nullopt;
  if (idp_display_data.idp_metadata.supports_add_account) {
    use_other_account_callback = base::BindRepeating(
        &AccountSelectionViewBase::Observer::OnLoginToIdP,
        base::Unretained(observer_), idp_display_data.idp_metadata.config_url,
        idp_display_data.idp_metadata.idp_login_url);
  }
  AddChildView(CreateButtonRow(/*continue_callback=*/std::nullopt,
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
  AddChildView(CreateButtonRow());

  InitDialogWidget();
}

void AccountSelectionModalView::ShowRequestPermissionDialog(
    const std::u16string& top_frame_for_display,
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data) {
  RemoveNonHeaderChildViews();

  ConfigureBrandImageView(brand_icon_,
                          idp_display_data.idp_metadata.brand_icon_url);

  // Hide the "Choose an account to continue" label.
  CHECK(body_label_);
  body_label_->SetVisible(/*visible=*/false);

  account_chooser_ = AddChildView(CreateSingleAccountChooser(
      idp_display_data, account,
      /*should_hover=*/false,
      /*show_disclosure_label=*/account.login_state ==
          Account::LoginState::kSignUp,
      /*show_separator=*/false,
      /*additional_row_vertical_padding=*/0));
  AddChildView(CreateButtonRow(
      base::BindRepeating(
          &AccountSelectionViewBase::Observer::OnAccountSelected,
          base::Unretained(observer_), std::cref(account),
          std::cref(idp_display_data)),
      /*use_other_account_callback=*/std::nullopt,
      base::BindRepeating(
          &AccountSelectionViewBase::Observer::OnBackButtonClicked,
          base::Unretained(observer_))));

  InitDialogWidget();
}

void AccountSelectionModalView::ShowSingleReturningAccountDialog(
    const std::vector<IdentityProviderDisplayData>& idp_data_list) {
  NOTREACHED() << "ShowSingleReturningAccountDialog is only implemented for "
                  "AccountSelectionBubbleView";
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateBrandIconImageView(
    const GURL& brand_icon_url) {
  // Create IDP brand icon image view.
  std::unique_ptr<BrandIconImageView> brand_icon_image_view =
      std::make_unique<BrandIconImageView>(
          base::BindOnce(&AccountSelectionViewBase::AddIdpImage,
                         weak_ptr_factory_.GetWeakPtr()),
          kModalIdpIconSize, /*should_circle_crop=*/false);
  brand_icon_ = brand_icon_image_view.get();
  brand_icon_image_view->SetImageSize(
      gfx::Size(kModalIdpIconSize, kModalIdpIconSize));
  if (brand_icon_url.is_valid()) {
    ConfigureBrandImageView(brand_icon_image_view.get(), brand_icon_url);
  } else {
    brand_icon_image_view->SetImage(ui::ImageModel::FromVectorIcon(
        kWebidGlobeIcon, ui::kColorIconSecondary, kModalIdpIconSize));
    brand_icon_image_view->SetVisible(true);
  }

  // Create background image view.
  constexpr int kBackgroundWidth = 408;
  constexpr int kBackgroundHeight = 100;
  const bool is_dark_mode = color_utils::IsDark(
      web_contents_->GetColorProvider().GetColor(ui::kColorDialogBackground));
  gfx::ImageSkia* background =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          is_dark_mode ? IDR_WEBID_MODAL_ICON_BACKGROUND_DARK
                       : IDR_WEBID_MODAL_ICON_BACKGROUND_LIGHT);
  std::unique_ptr<views::ImageView> background_image_view =
      std::make_unique<views::ImageView>();
  background_image_view->SetImage(*background);
  background_image_view->SetImageSize(
      gfx::Size(kBackgroundWidth, kBackgroundHeight));

  // Put background image view into a FillLayout container.
  std::unique_ptr<views::View> background_container =
      std::make_unique<views::View>();
  background_container->SetUseDefaultFillLayout(true);
  background_container->AddChildView(std::move(background_image_view));

  // Put brand icon image view into a BoxLayout container.
  std::unique_ptr<views::BoxLayoutView> icon_container =
      std::make_unique<views::BoxLayoutView>();
  icon_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  icon_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  icon_container->AddChildView(std::move(brand_icon_image_view));

  // Put BoxLayout container into FillLayout container to stack the views. This
  // stacks the IDP icon on top of the background image.
  background_container->AddChildView(std::move(icon_container));

  return background_container;
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

std::u16string AccountSelectionModalView::GetQueuedAnnouncementForTesting() {
  return queued_announcement_;
}

views::View* AccountSelectionModalView::GetInitiallyFocusedView() {
  // If title has not been announced before, focus and announce the title.
  if (!has_announced_title_) {
    has_announced_title_ = true;
    return title_label_;
  }

  // Make the queued announcement, if available. This can either be the
  // disclosure text or the verifying status.
  if (!queued_announcement_.empty()) {
    GetViewAccessibility().AnnounceAlert(queued_announcement_);
    queued_announcement_ = u"";
  }

  // If there is a progress bar and an account chooser, we are on the verifying
  // sheet so focus on the cancel button.
  if (has_progress_bar_ && account_chooser_) {
    return cancel_button_;
  }

  // If there is a continue button, focus on the continue button.
  if (continue_button_) {
    return continue_button_;
  }

  // Default to the title.
  return title_label_;
}

void AccountSelectionModalView::RemoveNonHeaderChildViews() {
  // If removing progress bar, adjust the header margins so the rest of the
  // dialog doesn't get shifted when the progress bar is removed.
  if (has_progress_bar_) {
    CHECK(header_view_);
    static_cast<views::BoxLayout*>(header_view_->GetLayoutManager())
        ->set_inside_border_insets(gfx::Insets::TLBR(
            /*top=*/kDialogMargin, /*left=*/kDialogMargin,
            /*bottom=*/kVerticalPadding, /*right=*/kDialogMargin));
    has_progress_bar_ = false;
  }

  // Make sure not to keep dangling pointers around first. We do not reset
  // `header_view_`, `title_label_`, `body_label_` and `brand_icon_` because
  // this method does not remove the header.
  use_other_account_button_ = nullptr;
  back_button_ = nullptr;
  continue_button_ = nullptr;
  cancel_button_ = nullptr;
  account_chooser_ = nullptr;

  const std::vector<raw_ptr<views::View, VectorExperimental>> child_views =
      children();
  for (views::View* child_view : child_views) {
    if (child_view != header_view_) {
      RemoveChildView(child_view);
      delete child_view;
    }
  }
}

BEGIN_METADATA(AccountSelectionModalView)
END_METADATA
