// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"

#include <iostream>
#include <memory>
#include <optional>
#include <utility>

#include "base/barrier_closure.h"
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
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
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

// The size of the spacing used between most children elements.
constexpr int kBetweenChildSpacing = 4;
// The size of the vertical padding for most elements in the dialog.
constexpr int kVerticalPadding = 8;
// The width of the modal dialog.
constexpr int kDialogWidth = 448;
// The margins of the modal dialog.
constexpr int kDialogMargin = 20;

class BackgroundImageView : public views::ImageView {
  METADATA_HEADER(BackgroundImageView, views::ImageView)

 public:
  explicit BackgroundImageView(base::WeakPtr<content::WebContents> web_contents)
      : web_contents_(web_contents) {
    constexpr int kBackgroundWidth = 408;
    constexpr int kBackgroundHeight = 100;
    SetImageSize(gfx::Size(kBackgroundWidth, kBackgroundHeight));
    UpdateBackgroundImage();
  }

  BackgroundImageView(const BackgroundImageView&) = delete;
  BackgroundImageView& operator=(const BackgroundImageView&) = delete;
  ~BackgroundImageView() override = default;

  void UpdateBackgroundImage() {
    CHECK(web_contents_);
    const bool is_dark_mode = color_utils::IsDark(
        web_contents_->GetColorProvider().GetColor(ui::kColorDialogBackground));
    gfx::ImageSkia* background =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            is_dark_mode ? IDR_WEBID_MODAL_ICON_BACKGROUND_DARK
                         : IDR_WEBID_MODAL_ICON_BACKGROUND_LIGHT);
    SetImage(*background);
  }

  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    UpdateBackgroundImage();
  }

 private:
  // Web contents is used to determine whether to show the light or dark mode
  // image.
  base::WeakPtr<content::WebContents> web_contents_;
};

BEGIN_METADATA(BackgroundImageView)
END_METADATA

AccountSelectionModalView::AccountSelectionModalView(
    const std::u16string& rp_for_display,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    content::WebContents* web_contents,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AccountSelectionViewBase::Observer* observer,
    views::WidgetObserver* widget_observer)
    : AccountSelectionViewBase(web_contents,
                               observer,
                               widget_observer,
                               std::move(url_loader_factory),
                               rp_for_display) {
  SetModalType(ui::mojom::ModalType::kChild);
  SetOwnedByWidget(true);
  set_fixed_width(kDialogWidth);
  SetShowTitle(false);
  SetShowCloseButton(false);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kBetweenChildSpacing));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  title_ = webid::GetTitle(rp_for_display_, idp_title, rp_context);
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

void AccountSelectionModalView::UpdateDialogPosition() {
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(), web_modal::WebContentsModalDialogManager::FromWebContents(
                       web_contents_.get())
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
    UpdateDialogPosition();
    return;
  }

  // Create and show the dialog widget. This is functionally a tab-modal dialog.
  // Showing and hiding is done by FedCmAccountSelectionView. See
  // https://crbug.com/364926910 for details.
  gfx::NativeWindow top_level_native_window =
      web_contents_->GetTopLevelNativeWindow();
  views::Widget* top_level_widget =
      views::Widget::GetWidgetForNativeWindow(top_level_native_window);
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      this, /*context=*/nullptr, /*parent=*/top_level_widget->GetNativeView());
  widget->Show();
  UpdateDialogPosition();

  // Add the widget observer, if available. It is null in tests.
  if (widget_observer_) {
    widget->AddObserver(widget_observer_);
  }

  dialog_widget_ = widget->GetWeakPtr();
  occlusion_observation_.Observe(widget);
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreatePlaceholderAccountRow() {
  const SkColor kPlaceholderColor =
      color_utils::IsDark(web_contents_->GetColorProvider().GetColor(
          ui::kColorDialogBackground))
          ? gfx::kGoogleGrey800
          : gfx::kGoogleGrey200;

  std::unique_ptr<views::View> placeholder_account_icon =
      std::make_unique<views::View>();
  placeholder_account_icon->SetPreferredSize(
      gfx::Size(kModalAvatarSize, kModalAvatarSize));
  placeholder_account_icon->SizeToPreferredSize();
  placeholder_account_icon->SetBackground(
      views::CreateRoundedRectBackground(kPlaceholderColor, kModalAvatarSize));

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
      kPlaceholderColor, kPlaceholderRadius));

  views::View* placeholder_account_email =
      text_column->AddChildView(std::make_unique<views::View>());
  placeholder_account_email->SetPreferredSize(
      gfx::Size(kPlaceholderAccountEmailWidth, kPlaceholderTextHeight));
  placeholder_account_email->SizeToPreferredSize();
  placeholder_account_email->SetBackground(views::CreateRoundedRectBackground(
      kPlaceholderColor, kPlaceholderRadius));

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

std::unique_ptr<views::View> AccountSelectionModalView::CreateHeader() {
  std::unique_ptr<views::View> header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(/*top=*/kDialogMargin, /*left=*/kDialogMargin,
                        /*bottom=*/kVerticalPadding, /*right=*/kDialogMargin),
      /*between_child_spacing=*/kVerticalPadding));

  // Add background image and IDP icon container.
  header_icon_view_ = header->AddChildView(CreateIconHeaderView());

  // Add the title.
  title_label_ = header->AddChildView(
      std::make_unique<views::Label>(title_, views::style::CONTEXT_DIALOG_TITLE,
                                     views::style::STYLE_HEADLINE_4));
  SetLabelProperties(title_label_);
  title_label_->SetFocusBehavior(FocusBehavior::ALWAYS);

  return header;
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateMultipleAccountChooser(
    const std::vector<IdentityRequestAccountPtr>& accounts) {
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

  int num_rows = 0;
  constexpr int kMultipleAccountsVerticalPadding = 2;
  for (const auto& account : accounts) {
    content->AddChildView(CreateAccountRow(
        *account,
        /*clickable_position=*/num_rows++,
        /*should_include_idp=*/false,
        /*is_modal_dialog=*/true,
        /*additional_vertical_padding=*/kMultipleAccountsVerticalPadding));
    // Add separator after each account row.
    content->AddChildView(std::make_unique<views::Separator>());
  }

  const int per_account_size = content->GetPreferredSize().height() / num_rows;
  scroll_view->ClipHeightTo(0, static_cast<int>(per_account_size * 3.5f));
  return scroll_view;
}

void AccountSelectionModalView::ShowMultiAccountPicker(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    bool show_back_button,
    bool is_choose_an_account) {
  DCHECK(!show_back_button);
  RemoveNonHeaderChildViewsAndUpdateHeaderIfNeeded();

  GURL idp_brand_icon_url = idp_list[0]->idp_metadata.brand_icon_url;
  // If `idp_brand_icon_url` is invalid, a globe icon is shown instead.
  if (idp_brand_icon_url.is_valid()) {
    ConfigureBrandImageView(idp_brand_icon_, idp_brand_icon_url);
  } else {
    idp_brand_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        kWebidGlobeIcon, ui::kColorIconSecondary, kModalIdpIconSize));
  }
  idp_brand_icon_->SetVisible(/*visible=*/true);

  // `combined_icons_` is created in `ShowRequestPermissionDialog` and is only
  // meant to be shown there, but it might be present here when the user clicks
  // the back button.
  MaybeRemoveCombinedIconsView();

  // Show the "Choose an account to continue" label.
  CHECK(body_label_);
  body_label_->SetVisible(/*visible=*/true);

  account_chooser_ = AddChildView(CreateMultipleAccountChooser(accounts));

  std::optional<views::Button::PressedCallback> use_other_account_callback =
      std::nullopt;

  // TODO(crbug.com/324052630): Support add account with multi IDP API.
  if (idp_list[0]->idp_metadata.supports_add_account) {
    use_other_account_callback = base::BindRepeating(
        &AccountSelectionViewBase::Observer::OnLoginToIdP,
        base::Unretained(observer_), idp_list[0]->idp_metadata.config_url,
        idp_list[0]->idp_metadata.idp_login_url);
  }
  AddChildView(CreateButtonRow(/*continue_callback=*/std::nullopt,
                               std::move(use_other_account_callback)));

  InitDialogWidget();

  // TODO(crbug.com/324052630): Connect with multi IDP API.
}

void AccountSelectionModalView::ShowVerifyingSheet(
    const content::IdentityRequestAccount& account,
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
  row->AddChildView(CreateAccountRow(
      account, should_hover ? std::make_optional<int>(0) : std::nullopt,
      /*should_include_idp=*/false,
      /*is_modal_dialog=*/true, additional_row_vertical_padding));

  // Add separator after the account row.
  if (show_separator) {
    row->AddChildView(std::make_unique<views::Separator>());
  }

  // Add disclosure label.
  if (show_disclosure_label) {
    std::unique_ptr<views::StyledLabel> disclosure_label =
        CreateDisclosureLabel(*account.identity_provider);
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
    const content::IdentityRequestAccount& account,
    bool show_back_button) {
  RemoveNonHeaderChildViewsAndUpdateHeaderIfNeeded();

  const content::IdentityProviderData& idp_data = *account.identity_provider;
  GURL idp_brand_icon_url = idp_data.idp_metadata.brand_icon_url;
  // If `idp_brand_icon_url` is invalid, a globe icon is shown instead.
  if (idp_brand_icon_url.is_valid()) {
    ConfigureBrandImageView(idp_brand_icon_, idp_brand_icon_url);
  } else {
    idp_brand_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        kWebidGlobeIcon, ui::kColorIconSecondary, kModalIdpIconSize));
  }
  idp_brand_icon_->SetVisible(/*visible=*/true);

  // `combined_icons_` is created in `ShowRequestPermissionDialog` and is only
  // meant to be shown there, but it might be present here when the user clicks
  // the back button.
  MaybeRemoveCombinedIconsView();

  // Show the "Choose an account to continue" label.
  CHECK(body_label_);
  body_label_->SetVisible(/*visible=*/true);

  account_chooser_ = AddChildView(CreateSingleAccountChooser(
      account,
      /*should_hover=*/true,
      /*show_disclosure_label=*/false,
      /*show_separator=*/true,
      /*additional_row_vertical_padding=*/kVerticalPadding));

  std::optional<views::Button::PressedCallback> use_other_account_callback =
      std::nullopt;
  if (idp_data.idp_metadata.supports_add_account) {
    use_other_account_callback = base::BindRepeating(
        &AccountSelectionViewBase::Observer::OnLoginToIdP,
        base::Unretained(observer_), idp_data.idp_metadata.config_url,
        idp_data.idp_metadata.idp_login_url);
  }
  AddChildView(CreateButtonRow(/*continue_callback=*/std::nullopt,
                               std::move(use_other_account_callback)));

  InitDialogWidget();

  // TODO(crbug.com/324052630): Connect with multi IDP API.
}

void AccountSelectionModalView::ShowFailureDialog(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  NOTREACHED_IN_MIGRATION()
      << "ShowFailureDialog is only implemented for AccountSelectionBubbleView";
}

void AccountSelectionModalView::ShowErrorDialog(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  NOTREACHED_IN_MIGRATION()
      << "ShowErrorDialog is only implemented for AccountSelectionBubbleView";
}

void AccountSelectionModalView::ShowLoadingDialog() {
  header_view_ = AddChildView(CreateHeader());
  AddChildView(CreatePlaceholderAccountRow());
  AddChildView(CreateButtonRow());

  InitDialogWidget();
}

void AccountSelectionModalView::OnIdpBrandIconFetched() {
  if (!idp_brand_icon_) {
    return;
  }
  header_icon_spinner_->Stop();
  header_icon_spinner_->SetVisible(/*visible=*/false);
  idp_brand_icon_->SetVisible(/*visible=*/true);
}

void AccountSelectionModalView::OnCombinedIconsFetched() {
  if (!combined_icons_) {
    return;
  }
  header_icon_spinner_->Stop();
  header_icon_spinner_->SetVisible(/*visible=*/false);
  idp_brand_icon_->SetVisible(/*visible=*/false);
  combined_icons_->SetVisible(/*visible=*/true);
}

void AccountSelectionModalView::ShowRequestPermissionDialog(
    const content::IdentityRequestAccount& account,
    const content::IdentityProviderData& idp_data) {
  RemoveNonHeaderChildViewsAndUpdateHeaderIfNeeded();

  GURL idp_brand_icon_url = idp_data.idp_metadata.brand_icon_url;
  GURL rp_brand_icon_url = idp_data.client_metadata.brand_icon_url;
  // Show RP icon if and only if both IDP and RP icons are available. The
  // combined icons view is only made visible when both IDP and RP icon fetches
  // succeed.
  if (idp_brand_icon_url.is_valid() && rp_brand_icon_url.is_valid()) {
    combined_icons_ =
        header_icon_view_->AddChildView(CreateCombinedIconsView());
    ConfigureBrandImageView(combined_icons_idp_brand_icon_, idp_brand_icon_url);
    ConfigureBrandImageView(combined_icons_rp_brand_icon_, rp_brand_icon_url);
  } else {
    // If `idp_brand_icon_url` is invalid, a globe icon is shown instead.
    ConfigureBrandImageView(idp_brand_icon_, idp_brand_icon_url);
    idp_brand_icon_->SetVisible(/*visible=*/true);
  }

  // Hide the "Choose an account to continue" label.
  CHECK(body_label_);
  body_label_->SetVisible(/*visible=*/false);

  account_chooser_ = AddChildView(CreateSingleAccountChooser(
      account,
      /*should_hover=*/false,
      /*show_disclosure_label=*/account.login_state ==
          Account::LoginState::kSignUp,
      /*show_separator=*/false,
      /*additional_row_vertical_padding=*/0));
  AddChildView(CreateButtonRow(
      base::BindRepeating(
          &AccountSelectionViewBase::Observer::OnAccountSelected,
          base::Unretained(observer_), std::cref(account), std::cref(idp_data)),
      /*use_other_account_callback=*/std::nullopt,
      base::BindRepeating(
          &AccountSelectionViewBase::Observer::OnBackButtonClicked,
          base::Unretained(observer_))));

  InitDialogWidget();
}

void AccountSelectionModalView::ShowSingleReturningAccountDialog(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list) {
  NOTREACHED_IN_MIGRATION()
      << "ShowSingleReturningAccountDialog is only implemented for "
         "AccountSelectionBubbleView";
}

std::unique_ptr<views::View> AccountSelectionModalView::CreateIconHeaderView() {
  // Create background image view.
  std::unique_ptr<BackgroundImageView> background_image_view =
      std::make_unique<BackgroundImageView>(web_contents_);

  // Put background image view into a FillLayout container.
  std::unique_ptr<views::View> background_container =
      std::make_unique<views::View>();
  background_container->SetUseDefaultFillLayout(true);
  background_container->AddChildView(std::move(background_image_view));

  // Put BoxLayout containers into FillLayout container to stack the views. This
  // stacks the spinner and icon container on top of the background image.
  background_container->AddChildView(CreateSpinnerIconView());
  background_container->AddChildView(CreateIdpIconView());

  return background_container;
}

std::unique_ptr<views::BoxLayoutView>
AccountSelectionModalView::CreateSpinnerIconView() {
  // Put spinner icon into a BoxLayout container so that it can be stacked on
  // top of the background.
  std::unique_ptr<views::BoxLayoutView> icon_container =
      std::make_unique<views::BoxLayoutView>();
  icon_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  icon_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  std::unique_ptr<views::Throbber> header_icon_spinner =
      std::make_unique<views::Throbber>();
  header_icon_spinner->SetPreferredSize(
      gfx::Size(kModalIconSpinnerSize, kModalIconSpinnerSize));
  header_icon_spinner->Start();
  header_icon_spinner_ =
      icon_container->AddChildView(std::move(header_icon_spinner));

  return icon_container;
}

std::unique_ptr<views::BoxLayoutView>
AccountSelectionModalView::CreateIdpIconView() {
  constexpr int kNumIconsInIdpIconView = 1;
  base::RepeatingClosure on_image_set = BarrierClosure(
      kNumIconsInIdpIconView,
      base::BindOnce(&AccountSelectionModalView::OnIdpBrandIconFetched,
                     weak_ptr_factory_.GetWeakPtr()));

  // Create IDP brand icon image view.
  std::unique_ptr<BrandIconImageView> idp_brand_icon_image_view =
      std::make_unique<BrandIconImageView>(
          base::BindOnce(&AccountSelectionViewBase::AddIdpImage,
                         weak_ptr_factory_.GetWeakPtr()),
          kModalIdpIconSize, /*should_circle_crop=*/true,
          /*background_color=*/std::nullopt, on_image_set);
  idp_brand_icon_image_view->SetImageSize(
      gfx::Size(kModalIdpIconSize, kModalIdpIconSize));
  idp_brand_icon_image_view->SetVisible(/*visible=*/false);

  // Put IDP icon into a BoxLayout container so that it can be stacked on top of
  // the background.
  std::unique_ptr<views::BoxLayoutView> icon_container =
      std::make_unique<views::BoxLayoutView>();
  icon_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  icon_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  idp_brand_icon_ =
      icon_container->AddChildView(std::move(idp_brand_icon_image_view));

  return icon_container;
}

std::unique_ptr<views::BoxLayoutView>
AccountSelectionModalView::CreateCombinedIconsView() {
  constexpr int kNumIconsInCombinedIconsView = 2;
  base::RepeatingClosure on_image_set = BarrierClosure(
      kNumIconsInCombinedIconsView,
      base::BindOnce(&AccountSelectionModalView::OnCombinedIconsFetched,
                     weak_ptr_factory_.GetWeakPtr()));

  // Create IDP brand icon image view.
  std::unique_ptr<BrandIconImageView> idp_brand_icon_image_view =
      std::make_unique<BrandIconImageView>(
          base::BindOnce(&AccountSelectionViewBase::AddIdpImage,
                         weak_ptr_factory_.GetWeakPtr()),
          kModalCombinedIconSize, /*should_circle_crop=*/true,
          /*background_color=*/std::nullopt, on_image_set);
  combined_icons_idp_brand_icon_ = idp_brand_icon_image_view.get();
  idp_brand_icon_image_view->SetImageSize(
      gfx::Size(kModalCombinedIconSize, kModalCombinedIconSize));
  idp_brand_icon_image_view->SetVisible(/*visible=*/true);

  // Create arrow icon image view.
  std::unique_ptr<views::ImageView> arrow_icon_image_view =
      std::make_unique<views::ImageView>();
  arrow_icon_image_view->SetImage(ui::ImageModel::FromVectorIcon(
      kWebidArrowIcon, ui::kColorIconSecondary, kModalCombinedIconSize));

  // Create RP brand icon image view.
  std::unique_ptr<BrandIconImageView> rp_brand_icon_image_view =
      std::make_unique<BrandIconImageView>(
          base::BindOnce(&AccountSelectionViewBase::AddIdpImage,
                         weak_ptr_factory_.GetWeakPtr()),
          kModalCombinedIconSize, /*should_circle_crop=*/true,
          /*background_color=*/std::nullopt, on_image_set);
  combined_icons_rp_brand_icon_ = rp_brand_icon_image_view.get();
  rp_brand_icon_image_view->SetImageSize(
      gfx::Size(kModalCombinedIconSize, kModalCombinedIconSize));
  rp_brand_icon_image_view->SetVisible(/*visible=*/true);

  // Put IDP icon, arrow icon and RP icon into a BoxLayout container, in that
  // order. This is so that they can be stacked on top of the background.
  std::unique_ptr<views::BoxLayoutView> icon_container =
      std::make_unique<views::BoxLayoutView>();
  icon_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  icon_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  icon_container->SetBetweenChildSpacing(kBetweenChildSpacing);
  icon_container->AddChildView(std::move(idp_brand_icon_image_view));
  icon_container->AddChildView(std::move(arrow_icon_image_view));
  icon_container->AddChildView(std::move(rp_brand_icon_image_view));
  icon_container->SetVisible(/*visible=*/false);

  return icon_container;
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

void AccountSelectionModalView::
    RemoveNonHeaderChildViewsAndUpdateHeaderIfNeeded() {
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

  // body_label_ does not apply to the loading modal so it's added to header
  // here.
  if (!body_label_) {
    body_label_ = header_view_->AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_CHOOSE_AN_ACCOUNT),
        views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_4));
    SetLabelProperties(body_label_);
    body_label_->SetFocusBehavior(FocusBehavior::ALWAYS);
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

void AccountSelectionModalView::MaybeRemoveCombinedIconsView() {
  if (!combined_icons_) {
    return;
  }

  // Make sure not to keep dangling pointers around first.
  combined_icons_idp_brand_icon_ = nullptr;
  combined_icons_rp_brand_icon_ = nullptr;
  combined_icons_->RemoveAllChildViews();
  combined_icons_.ClearAndDelete();
}

BEGIN_METADATA(AccountSelectionModalView)
END_METADATA
