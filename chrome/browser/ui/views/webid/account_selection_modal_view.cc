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
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/browser/ui/views/webid/webid_utils.h"
#include "chrome/browser/ui/webid/identity_ui_utils.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
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
#include "ui/views/window/dialog_delegate.h"

namespace webid {

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
    SetImage(ui::ImageModel::FromResourceId(
        is_dark_mode ? IDR_WEBID_MODAL_ICON_BACKGROUND_DARK
                     : IDR_WEBID_MODAL_ICON_BACKGROUND_LIGHT));
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

namespace {
std::unique_ptr<views::View> CreateButtonContainer() {
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
  return button_container;
}
}  // namespace

AccountSelectionModalDelegate::AccountSelectionModalDelegate(
    std::unique_ptr<AccountSelectionModalView> account_selection_modal_view) {
  auto* selection_modal =
      SetContentsView(std::move(account_selection_modal_view));
  SetModalType(ui::mojom::ModalType::kChild);
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  set_fixed_width(kDialogWidth);
  SetShowTitle(false);
  SetShowCloseButton(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetTitle(selection_modal->dialog_title());
}

AccountSelectionModalDelegate::~AccountSelectionModalDelegate() = default;

views::View* AccountSelectionModalDelegate::GetInitiallyFocusedView() {
  if (auto* initially_focused_view =
          GetAccountSelectionView()->GetInitiallyFocusedView()) {
    return initially_focused_view;
  }
  return views::DialogDelegate::GetInitiallyFocusedView();
}

views::Widget* AccountSelectionModalDelegate::GetWidget() {
  return GetAccountSelectionView()->GetWidget();
}

const views::Widget* AccountSelectionModalDelegate::GetWidget() const {
  return const_cast<AccountSelectionModalDelegate*>(this)
      ->GetAccountSelectionView()
      ->GetWidget();
}

AccountSelectionModalView*
AccountSelectionModalDelegate::GetAccountSelectionView() {
  if (auto* account_selection_modal_view =
          views::AsViewClass<AccountSelectionModalView>(GetContentsView())) {
    return account_selection_modal_view;
  }
  NOTREACHED()
      << "Dialog ContentsView isn't of type AccountSelectionModalView!";
}

AccountSelectionModalView::AccountSelectionModalView(
    const content::RelyingPartyData& rp_data,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FedCmAccountSelectionView* owner)
    : AccountSelectionViewBase(owner,
                               std::move(url_loader_factory),
                               rp_data,
                               owner->web_contents()
                                   ->GetPrimaryMainFrame()
                                   ->GetRenderWidgetHost()
                                   ->GetDeviceScaleFactor()),
      idp_title_(idp_title),
      rp_context_(rp_context) {
  // Configure the BoxLayoutView
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetBetweenChildSpacing(kBetweenChildSpacing);

  header_view_ = AddChildView(CreateHeader());
  UpdateTitleAndSubtitle(rp_data);

  AddChildView(CreatePlaceholderAccountRow());
  AddChildView(CreateButtonRow(/*continue_callback=*/std::nullopt,
                               /*use_other_account_callback=*/std::nullopt,
                               /*back_callback=*/std::nullopt));
}

AccountSelectionModalView::~AccountSelectionModalView() = default;

std::unique_ptr<views::View>
AccountSelectionModalView::CreatePlaceholderAccountRow() {
  const SkColor kPlaceholderColor =
      color_utils::IsDark(owner_->web_contents()->GetColorProvider().GetColor(
          ui::kColorDialogBackground))
          ? gfx::kGoogleGrey800
          : gfx::kGoogleGrey200;

  std::unique_ptr<views::View> placeholder_account_icon =
      std::make_unique<views::View>();
  placeholder_account_icon->SetPreferredSize(
      gfx::Size(webid::kModalAvatarSize, webid::kModalAvatarSize));
  placeholder_account_icon->SizeToPreferredSize();
  placeholder_account_icon->SetBackground(views::CreateRoundedRectBackground(
      kPlaceholderColor, webid::kModalAvatarSize));

  constexpr int kPlaceholderAccountRowPadding = 16;
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(
          /*vertical=*/kPlaceholderAccountRowPadding,
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
    std::optional<views::Button::PressedCallback> continue_callback,
    std::optional<views::Button::PressedCallback> use_other_account_callback,
    std::optional<views::Button::PressedCallback> back_callback) {
  std::unique_ptr<views::View> button_container = CreateButtonContainer();

  std::unique_ptr<views::MdTextButton> cancel_button =
      std::make_unique<views::MdTextButton>(
          base::BindRepeating(&FedCmAccountSelectionView::OnCloseButtonClicked,
                              base::Unretained(owner_)),
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
  title_label_ = header->AddChildView(std::make_unique<views::Label>(
      u"", views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_HEADLINE_4));

  SetLabelProperties(title_label_);

  return header;
}

std::unique_ptr<views::View>
AccountSelectionModalView::CreateMultipleAccountChooser(
    const std::vector<IdentityRequestAccountPtr>& accounts) {
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  views::View* const content = scroll_view->SetContents(CreateAccountRows(
      accounts, /*should_hover=*/true, /*show_separator=*/true,
      /*is_request_permission_dialog=*/false));

  constexpr float kMaxAccountsToShow = 3.5f;
  const int per_account_size =
      content->GetPreferredSize().height() / accounts.size();
  scroll_view->ClipHeightTo(
      0, static_cast<int>(per_account_size * kMaxAccountsToShow));
  return scroll_view;
}

std::unique_ptr<views::View> AccountSelectionModalView::CreateAccountRows(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    bool should_hover,
    bool show_separator,
    bool is_request_permission_dialog) {
  auto content = std::make_unique<views::View>();
  content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(
          /*vertical=*/is_request_permission_dialog ? 0 : kVerticalPadding,
          /*horizontal=*/kDialogMargin)));

  if (show_separator) {
    // Add separator before the account rows.
    content->AddChildView(std::make_unique<views::Separator>());
  }

  int num_rows = 0;
  constexpr int kAccountRowVerticalPadding = 2;
  for (const auto& account : accounts) {
    content->AddChildView(CreateAccountRow(
        account,
        /*clickable_position=*/
        should_hover ? std::make_optional<int>(num_rows++) : std::nullopt,
        /*should_include_idp=*/false,
        /*is_modal_dialog=*/true,
        /*additional_vertical_padding=*/
        is_request_permission_dialog ? 0 : kAccountRowVerticalPadding));
    if (show_separator) {
      // Add separator after each account row.
      content->AddChildView(std::make_unique<views::Separator>());
    }
  }
  return content;
}

void AccountSelectionModalView::ShowMultiAccountPicker(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    const gfx::Image& rp_icon,
    bool show_back_button) {
  DCHECK(!show_back_button);
  CHECK_EQ(idp_list.size(), 1u);
  ShowAccounts(accounts, /*is_single_account_chooser=*/false);
}

void AccountSelectionModalView::ShowAccounts(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    bool is_single_account_chooser) {
  RemoveNonHeaderChildViewsAndUpdateHeaderIfNeeded();

  const content::IdentityProviderMetadata& idp_metadata =
      accounts[0]->identity_provider->idp_metadata;
  // If `brand_decoded_icon` is empty, a globe icon is shown instead.
  if (!idp_metadata.brand_decoded_icon.IsEmpty()) {
    if (idp_brand_icon_->SetBrandIconImage(idp_metadata.brand_decoded_icon,
                                           /*should_circle_crop=*/true)) {
      OnIdpBrandIconSet();
    }
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

  if (is_single_account_chooser) {
    CHECK_EQ(accounts.size(), 1u);
    account_chooser_ =
        AddChildView(CreateAccountRows(accounts,
                                       /*should_hover=*/true,
                                       /*show_separator=*/true,
                                       /*is_request_permission_dialog=*/false));
  } else {
    account_chooser_ = AddChildView(CreateMultipleAccountChooser(accounts));
  }

  std::optional<views::Button::PressedCallback> use_other_account_callback =
      std::nullopt;

  // TODO(crbug.com/324052630): Support add account with multi IDP API.
  if (idp_metadata.supports_add_account ||
      idp_metadata.has_filtered_out_account) {
    use_other_account_callback = base::BindRepeating(
        &AccountSelectionModalView::OnUseOtherAccountButtonClicked,
        base::Unretained(this), idp_metadata.config_url,
        idp_metadata.idp_login_url);
  }
  AddChildView(CreateButtonRow(/*continue_callback=*/std::nullopt,
                               std::move(use_other_account_callback),
                               /*back_callback=*/std::nullopt));

  // TODO(crbug.com/324052630): Connect with multi IDP API.
}

void AccountSelectionModalView::ShowVerifyingSheet(
    const IdentityRequestAccountPtr& account,
    const std::u16string& title) {
  // A different type of sheet must have been shown prior to ShowVerifyingSheet.
  // This might change if we choose to integrate auto re-authn with button mode.
  CHECK(owner_->GetDialogWidget());

  SendAccessibilityEvent(GetWidget(),
                         l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE));

  // Disable account chooser.
  CHECK(account_chooser_);
  bool is_single_account_chooser = false;
  for (const auto& child : account_chooser_->children()) {
    // If one of the immediate children is HoverButton, this is a single account
    // chooser.
    if (child->GetClassName() == "HoverButton") {
      is_single_account_chooser = true;
      AccountHoverButton* button = static_cast<AccountHoverButton*>(child);
      if (button->HasBeenClicked()) {
        has_spinner_ = true;
        button->ReplaceSecondaryViewWithSpinner();
        verifying_focus_view_ = button;
      } else {
        button->SetEnabled(false);
        button->SetDisabledOpacity();
      }
    }
  }

  // If no immediate HoverButton child was found, it means that this is a
  // multiple account chooser and the HoverButtons are embedded within a
  // ScrollView.
  if (!is_single_account_chooser) {
    views::View* wrapper = account_chooser_->children()[0];
    views::View* contents = wrapper->children()[0];
    for (const auto& child : contents->children()) {
      if (child->GetClassName() == "HoverButton") {
        AccountHoverButton* button = static_cast<AccountHoverButton*>(child);
        if (button->HasBeenClicked()) {
          has_spinner_ = true;
          button->ReplaceSecondaryViewWithSpinner();
          verifying_focus_view_ = button;
        } else {
          button->SetEnabled(false);
          button->SetDisabledOpacity();
        }
      }
    }
  }

  if (use_other_account_button_) {
    // If there is no spinner, either on any of the account rows or the continue
    // button, this verifying sheet must have been triggered as a result of use
    // other account so we show the spinner on this button.
    if (!has_spinner_) {
      verifying_focus_view_ = use_other_account_button_;
      ReplaceButtonWithSpinner(use_other_account_button_);
    } else {
      use_other_account_button_->SetEnabled(false);
    }
  }

  if (continue_button_) {
    // If there is no focus view specified at this point, it must be that the
    // user clicked on the continue button.
    if (!verifying_focus_view_) {
      verifying_focus_view_ = continue_button_;
    } else {
      continue_button_->SetEnabled(false);
    }
  }

  if (back_button_) {
    back_button_->SetEnabled(false);
  }

  has_spinner_ = true;
}

void AccountSelectionModalView::ShowSingleAccountConfirmDialog(
    const IdentityRequestAccountPtr& account,
    bool show_back_button) {
  std::vector<IdentityRequestAccountPtr> accounts = {account};
  ShowAccounts(accounts, /*is_single_account_chooser=*/true);
}

void AccountSelectionModalView::ShowFailureDialog(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  NOTREACHED()
      << "ShowFailureDialog is only implemented for AccountSelectionBubbleView";
}

void AccountSelectionModalView::ShowErrorDialog(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  RemoveNonHeaderChildViewsAndUpdateHeaderIfNeeded();

  std::u16string summary_text;
  std::u16string description_text;
  std::tie(summary_text, description_text) =
      GetErrorDialogText(error, idp_for_display);

  title_ = summary_text;
  title_label_->SetText(title_);
  title_label_->SetVisible(true);
  if (auto* widget = GetWidget()) {
    widget->widget_delegate()->SetTitle(title_);
  }

  // body_label_ may be invisible if the preceding UI is the disclosure UI. When
  // error is triggered directly from the loading UI in case of auto re-authn,
  // body_label_ is still present at this moment.
  body_label_->SetVisible(/*visible=*/true);
  body_label_->SetText(description_text);

  std::unique_ptr<views::View> button_container = CreateButtonContainer();
  // Add more details button.
  if (error && !error->url.is_empty()) {
    auto more_details_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&FedCmAccountSelectionView::OnMoreDetails,
                            base::Unretained(owner_)),
        l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DIALOG_MORE_DETAILS_BUTTON));
    button_container->AddChildView(std::move(more_details_button));
  }

  // Add got it button.
  auto got_it_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&FedCmAccountSelectionView::OnGotIt,
                          base::Unretained(owner_)),
      l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DIALOG_GOT_IT_BUTTON));
  got_it_button->SetStyle(ui::ButtonStyle::kProminent);

  button_container->AddChildView(std::move(got_it_button));

  AddChildView(std::move(button_container));
}

void AccountSelectionModalView::OnIdpBrandIconSet() {
  header_icon_spinner_->Stop();
  header_icon_spinner_->SetVisible(false);
  idp_brand_icon_->SetVisible(true);
}

void AccountSelectionModalView::OnCombinedIconsSet() {
  header_icon_spinner_->Stop();
  header_icon_spinner_->SetVisible(false);
  idp_brand_icon_->SetVisible(false);
  combined_icons_->SetVisible(true);
}

void AccountSelectionModalView::ShowRequestPermissionDialog(
    const IdentityRequestAccountPtr& account) {
  RemoveNonHeaderChildViewsAndUpdateHeaderIfNeeded();

  const content::IdentityProviderData& idp_data = *account->identity_provider;
  const gfx::Image& idp_brand_icon = idp_data.idp_metadata.brand_decoded_icon;
  const gfx::Image& rp_brand_icon = idp_data.client_metadata.brand_decoded_icon;
  // Show RP icon if and only if both IDP and RP icons are available. The
  // combined icons view is only made visible when both IDP and RP icon fetches
  // succeed.
  if (!idp_brand_icon.IsEmpty() && !rp_brand_icon.IsEmpty()) {
    combined_icons_ =
        header_icon_view_->AddChildView(CreateCombinedIconsView());
    bool idp_icon_set = combined_icons_idp_brand_icon_->SetBrandIconImage(
        idp_brand_icon, /*should_circle_crop=*/true);
    bool rp_icon_set = combined_icons_rp_brand_icon_->SetBrandIconImage(
        rp_brand_icon, /*should_circle_crop=*/true);
    if (idp_icon_set && rp_icon_set) {
      OnCombinedIconsSet();
    }
  } else {
    // If `idp_brand_icon` is empty, a globe icon is shown instead.
    if (!idp_brand_icon.IsEmpty()) {
      if (idp_brand_icon_->SetBrandIconImage(idp_brand_icon,
                                             /*should_circle_crop=*/true)) {
        OnIdpBrandIconSet();
      }
    } else {
      idp_brand_icon_->SetImage(ui::ImageModel::FromVectorIcon(
          kWebidGlobeIcon, ui::kColorIconSecondary, kModalIdpIconSize));
    }
    idp_brand_icon_->SetVisible(/*visible=*/true);
  }

  // Hide the "Choose an account to continue" label, but not if we are instead
  // showing the iframe text.
  CHECK(body_label_);
  if (subtitle_.empty()) {
    body_label_->SetVisible(/*visible=*/false);
  }

  std::vector<IdentityRequestAccountPtr> accounts = {account};
  account_chooser_ =
      AddChildView(CreateAccountRows(accounts,
                                     /*should_hover=*/false,
                                     /*show_separator=*/false,
                                     /*is_request_permission_dialog=*/true));
  // It must be that either the account's login state is kSignUp or that fields
  // are empty if the account's login state is kSignIn.
  CHECK(account->idp_claimed_login_state.value_or(
            account->browser_trusted_login_state) ==
            Account::LoginState::kSignUp ||
        account->fields.empty());
  if (!account->fields.empty()) {
    // Add disclosure label.
    std::unique_ptr<views::StyledLabel> disclosure_label =
        CreateDisclosureLabel(account);
    disclosure_label->SetDefaultTextStyle(views::style::STYLE_BODY_4);
    disclosure_label->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        /*top=*/kVerticalSpacing, /*left=*/0, /*bottom=*/0,
        /*right=*/0)));
    // Announce immediately if the view is showing.
    if (GetWidget()->IsVisible()) {
      GetViewAccessibility().AnnounceAlert(disclosure_label->GetText());
    } else {
      queued_announcement_ = disclosure_label->GetText();
    }
    account_chooser_->AddChildView(std::move(disclosure_label));
  }
  AddChildView(CreateButtonRow(
      base::BindRepeating(&AccountSelectionModalView::OnContinueButtonClicked,
                          base::Unretained(this), account),
      /*use_other_account_callback=*/std::nullopt,
      base::BindRepeating(&FedCmAccountSelectionView::OnBackButtonClicked,
                          base::Unretained(owner_))));
}

void AccountSelectionModalView::OnContinueButtonClicked(
    const IdentityRequestAccountPtr& account,
    const ui::Event& event) {
  // In the verifying sheet, we do not disable the continue button if it has a
  // spinner because otherwise the focus will land on the cancel button.
  // Since the button is not disabled, it is possible for the button to be
  // clicked again and we would ignore these future clicks.
  if (verifying_focus_view_) {
    return;
  }

  owner_->OnAccountSelected(account, event);
  has_spinner_ = true;

  ReplaceButtonWithSpinner(continue_button_,
                           ui::kColorButtonForegroundProminent,
                           ui::kColorButtonBackgroundProminent);
}

void AccountSelectionModalView::OnUseOtherAccountButtonClicked(
    const GURL& idp_config_url,
    const GURL& idp_login_url,
    const ui::Event& event) {
  // In the verifying sheet, we do not disable the use other account button if
  // it has a spinner because otherwise the focus will land on the cancel
  // button. The use other account button has a spinner if the user signs into a
  // returning account via the pop-up. Since the button is not disabled, it is
  // possible for the button to be clicked again and we would ignore these
  // future clicks.
  if (verifying_focus_view_) {
    return;
  }

  owner_->OnLoginToIdP(idp_config_url, idp_login_url, event);
}

std::unique_ptr<views::View> AccountSelectionModalView::CreateIconHeaderView() {
  // Create background image view.
  std::unique_ptr<BackgroundImageView> background_image_view =
      std::make_unique<BackgroundImageView>(
          owner_->web_contents()->GetWeakPtr());

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
  header_icon_spinner->SizeToPreferredSize();
  header_icon_spinner->Start();
  header_icon_spinner_ =
      icon_container->AddChildView(std::move(header_icon_spinner));

  return icon_container;
}

std::unique_ptr<views::BoxLayoutView>
AccountSelectionModalView::CreateIdpIconView() {
  // Create IDP brand icon image view.
  auto idp_brand_icon_image_view =
      std::make_unique<BrandIconImageView>(kModalIdpIconSize);
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
  // Create IDP brand icon image view.
  auto idp_brand_icon_image_view =
      std::make_unique<BrandIconImageView>(kModalCombinedIconSize);
  combined_icons_idp_brand_icon_ = idp_brand_icon_image_view.get();
  idp_brand_icon_image_view->SetVisible(/*visible=*/true);

  // Create arrow icon image view.
  std::unique_ptr<views::ImageView> arrow_icon_image_view =
      std::make_unique<views::ImageView>();
  arrow_icon_image_view->SetImage(ui::ImageModel::FromVectorIcon(
      kWebidArrowIcon, ui::kColorIconSecondary, kModalCombinedIconSize));

  // Create RP brand icon image view.
  auto rp_brand_icon_image_view =
      std::make_unique<BrandIconImageView>(kModalCombinedIconSize);
  combined_icons_rp_brand_icon_ = rp_brand_icon_image_view.get();
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

void AccountSelectionModalView::ReplaceButtonWithSpinner(
    views::MdTextButton* button,
    ui::ColorId spinner_color,
    ui::ColorId button_color) {
  std::unique_ptr<views::Throbber> button_spinner =
      std::make_unique<views::Throbber>();
  button_spinner->SetPreferredSize(
      gfx::Size(kModalButtonSpinnerSize, kModalButtonSpinnerSize));
  button_spinner->SizeToPreferredSize();
  button_spinner->SetColorId(spinner_color);
  button_spinner->Start();

  // Spinner is put into a BoxLayoutView so that it can be shown on top of the
  // button.
  std::unique_ptr<views::BoxLayoutView> spinner_container =
      std::make_unique<views::BoxLayoutView>();
  spinner_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  spinner_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  spinner_container->AddChildView(std::move(button_spinner));

  // Set button text color to be the same as its background color so that the
  // text is not visible and the size of the button doesn't change. Explicitly
  // set the vertical border to 0 because otherwise, the spinner cannot fit in
  // the button in some OSes.
  button->SetUseDefaultFillLayout(true);
  button->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(0, button->GetBorder()->GetInsets().left())));
  button->AddChildView(std::move(spinner_container));
  button->SetTextColor(HoverButton::ButtonState::STATE_DISABLED, button_color);
  button->SetEnabledTextColors(button_color);
  button->SetBgColorIdOverride(button_color);
}

std::string AccountSelectionModalView::GetDialogTitle() const {
  return base::UTF16ToUTF8(title_label_->GetText());
}

void AccountSelectionModalView::UpdateTitleAndSubtitle(
    const content::RelyingPartyData& rp_data) {
  AccountSelectionViewBase::UpdateTitleAndSubtitle(rp_data);

  // Don't set a title until we know the strings won't change anymore.
  if (rp_data.display_strings_may_change) {
    title_label_->SetVisible(false);
    return;
  }

  title_ = GetTitle(rp_data, idp_title_, rp_context_);
  subtitle_ = GetSubtitle(rp_data);
  title_label_->SetText(title_);
  title_label_->SetVisible(true);
  if (body_label_) {
    body_label_->SetText(subtitle_);
    body_label_->SetVisible(true);
  }
  // Otherwise, we will set the text when we create body_label_.
  if (auto* widget = GetWidget()) {
    widget->widget_delegate()->SetTitle(title_);
  }
}

std::optional<std::string> AccountSelectionModalView::GetDialogSubtitle()
    const {
  if (subtitle_.empty()) {
    return std::nullopt;
  }
  return base::UTF16ToUTF8(subtitle_);
}

void AccountSelectionModalView::VisibilityChanged(View* starting_from,
                                                  bool is_visible) {
  if (is_visible && !queued_announcement_.empty()) {
    GetViewAccessibility().AnnounceAlert(queued_announcement_);
    queued_announcement_ = u"";
  }
}

std::u16string AccountSelectionModalView::GetQueuedAnnouncementForTesting() {
  return queued_announcement_;
}

views::View* AccountSelectionModalView::GetInitiallyFocusedView() {
  // If there is a view that triggered the verifying sheet, focus on the last
  // clicked view.
  if (verifying_focus_view_) {
    return verifying_focus_view_;
  }

  // If there is a continue button, focus on the continue button.
  if (continue_button_) {
    return continue_button_;
  }

  // Return null to indicate to the delegate to use the delegate's super-class.
  return nullptr;
}

void AccountSelectionModalView::
    RemoveNonHeaderChildViewsAndUpdateHeaderIfNeeded() {
  // body_label_ does not apply to the loading modal so it's added to header
  // here.
  if (!body_label_) {
    std::u16string body_text =
        subtitle_.empty()
            ? l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_CHOOSE_AN_ACCOUNT)
            : subtitle_;
    body_label_ = header_view_->AddChildView(std::make_unique<views::Label>(
        body_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_4));
    SetLabelProperties(body_label_);
  }

  // Make sure not to keep dangling pointers around first. We do not need to
  // reset pointers to views in the header.
  use_other_account_button_ = nullptr;
  back_button_ = nullptr;
  continue_button_ = nullptr;
  cancel_button_ = nullptr;
  account_chooser_ = nullptr;
  verifying_focus_view_ = nullptr;

  const std::vector<raw_ptr<views::View, VectorExperimental>> child_views =
      children();
  for (views::View* child_view : child_views) {
    if (child_view != header_view_) {
      RemoveChildViewT(child_view);
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

}  // namespace webid
