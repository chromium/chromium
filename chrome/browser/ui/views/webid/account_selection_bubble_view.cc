// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/browser/ui/views/webid/webid_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_features.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace webid {

namespace {

constexpr int kSingleIdpUseOtherAccountButtonIconMargin = 5;
constexpr int kLoginButtonSeparatorLeftMargin = 64;
constexpr int kLoginButtonSeparatorRightMargin = 15;
constexpr float kNumVisibleRows = 2.5f;

// views::MdTextButton which:
// - Uses the passed-in `brand_background_color` based on whether the button
//   background contrasts sufficiently with dialog background.
// - If `brand_text_color` is not provided, computes the text color such that it
//   contrasts sufficiently with `brand_background_color`.
// - If `extra_accessible_text` is passed, appends this to the button's
// accessible name. This is useful when the user logs in via a popup window and
// cannot easily navigate the rest of the text in the dialog to confirm which is
// the account being used to login via FedCM.
class ContinueButton : public views::MdTextButton {
  METADATA_HEADER(ContinueButton, views::MdTextButton)

 public:
  ContinueButton(views::MdTextButton::PressedCallback callback,
                 const std::u16string& text,
                 const content::IdentityProviderMetadata& idp_metadata,
                 std::optional<std::u16string> extra_accessible_text)
      : views::MdTextButton(std::move(callback), text),
        brand_background_color_(idp_metadata.brand_background_color),
        brand_text_color_(idp_metadata.brand_text_color) {
    SetCornerRadius(kButtonRadius);
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetStyle(ui::ButtonStyle::kProminent);
    if (extra_accessible_text.has_value()) {
      GetViewAccessibility().SetName(text + u", " + *extra_accessible_text);
    }
  }

  ContinueButton(const ContinueButton&) = delete;
  ContinueButton& operator=(const ContinueButton&) = delete;
  ~ContinueButton() override = default;

  void OnThemeChanged() override {
    views::MdTextButton::OnThemeChanged();
    if (!brand_background_color_) {
      return;
    }

    const SkColor dialog_background_color =
        GetWidget()
            ->widget_delegate()
            ->AsBubbleDialogDelegate()
            ->background_color()
            .ResolveToSkColor(GetColorProvider());
    if (color_utils::GetContrastRatio(dialog_background_color,
                                      *brand_background_color_) <
        color_utils::kMinimumVisibleContrastRatio) {
      SetBgColorOverrideDeprecated(std::nullopt);
      SetEnabledTextColors(std::nullopt);
      return;
    }

    SetBgColorOverrideDeprecated(*brand_background_color_);
    SkColor text_color;
    if (brand_text_color_) {
      // IdpNetworkRequestManager ensures that `brand_text_color_` is only set
      // if it sufficiently contrasts with `brand_background_color_`.
      text_color = *brand_text_color_;
    } else {
      text_color = color_utils::BlendForMinContrast(GetCurrentTextColor(),
                                                    *brand_background_color_)
                       .color;
    }
    SetEnabledTextColors(text_color);
  }

 private:
  std::optional<SkColor> brand_background_color_;
  std::optional<SkColor> brand_text_color_;
};

BEGIN_METADATA(ContinueButton)
END_METADATA

// Adds a login button separator to the given `scroller_content`. Returns the
// separator size.
int AddLoginButtonSeparator(views::View* scroller_content,
                            bool is_multi_idp,
                            const std::unique_ptr<views::View>& button) {
  auto separator = std::make_unique<views::Separator>();
  separator->SetBorder(views::CreateEmptyBorder(
      is_multi_idp ? gfx::Insets::TLBR(
                         kTopBottomPadding, kLoginButtonSeparatorLeftMargin,
                         kTopBottomPadding, kLoginButtonSeparatorRightMargin)
                   : gfx::Insets::VH(kVerticalSpacing + kTopBottomPadding, 0)));
  int separator_size = separator->GetPreferredSize().height();
  scroller_content->AddChildView(std::move(separator));
  return separator_size;
}

}  // namespace

AccountSelectionBubbleDelegate::AccountSelectionBubbleDelegate(
    std::unique_ptr<AccountSelectionBubbleView> account_selection_view,
    views::View* anchor_view)
    : views::BubbleDialogDelegate(
          anchor_view,
          // Note that TOP_RIGHT means the bubble's top and right are anchored
          // to the `anchor_view`. The final bubble positioning will be computed
          // in GetBubbleBounds.
          views::BubbleBorder::Arrow::TOP_RIGHT,
          views::BubbleBorder::DIALOG_SHADOW,
          /*autosize=*/true) {
  auto* selection_view = SetContentsView(std::move(account_selection_view));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_fixed_width(kBubbleWidth);
  SetShowTitle(false);
  SetShowCloseButton(false);
  set_close_on_deactivate(false);
  set_margins(gfx::Insets::VH(kTopBottomPadding + kVerticalSpacing, 0));
  SetAccessibleTitle(selection_view->dialog_title());
}

AccountSelectionBubbleDelegate::~AccountSelectionBubbleDelegate() = default;

gfx::Rect AccountSelectionBubbleDelegate::GetBubbleBounds() {
  gfx::Rect proposed_bubble_bounds =
      views::BubbleDialogDelegate::GetBubbleBounds();
  return GetAccountSelectionView()->GetBubbleBounds(proposed_bubble_bounds);
}

views::Widget* AccountSelectionBubbleDelegate::GetWidget() {
  return GetAccountSelectionView()->GetWidget();
}

const views::Widget* AccountSelectionBubbleDelegate::GetWidget() const {
  return const_cast<AccountSelectionBubbleDelegate*>(this)
      ->GetAccountSelectionView()
      ->GetWidget();
}

AccountSelectionBubbleView*
AccountSelectionBubbleDelegate::GetAccountSelectionView() {
  if (auto* account_selection_view_contents =
          views::AsViewClass<AccountSelectionBubbleView>(GetContentsView())) {
    return account_selection_view_contents;
  }
  NOTREACHED()
      << "Bubble ContentsView isn't of type AccountSelectionBubbleView!";
}

AccountSelectionBubbleView::AccountSelectionBubbleView(
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
      rp_context_(rp_context) {
  // Configure the BoxLayout
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetBetweenChildSpacing(kTopBottomPadding);

  title_ = GetTitle(rp_data_, idp_title, rp_context);

  header_view_ = AddChildView(CreateHeaderView());
}

AccountSelectionBubbleView::~AccountSelectionBubbleView() = default;

void AccountSelectionBubbleView::ShowMultiAccountPicker(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    const gfx::Image& rp_icon,
    bool show_back_button) {
  bool is_multi_idp = idp_list.size() > 1u;
  std::u16string title = GetTitle(
      rp_data_,
      is_multi_idp ? std::nullopt
                   : std::make_optional<std::u16string>(
                         base::UTF8ToUTF16(idp_list[0]->idp_for_display)),
      rp_context_);
  UpdateHeader(
      is_multi_idp ? rp_icon : idp_list[0]->idp_metadata.brand_decoded_icon,
      title, webid::GetSubtitle(rp_data_), show_back_button,
      /*should_circle_crop_header_icon=*/!is_multi_idp);

  RemoveNonHeaderChildViews();
  AddSeparatorAndMultipleAccountChooser(accounts, idp_list);

  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowVerifyingSheet(
    const IdentityRequestAccountPtr& account,
    const std::u16string& title) {
  UpdateHeader(account->identity_provider->idp_metadata.brand_decoded_icon,
               title, u"",
               /*show_back_button=*/false,
               /*should_circle_crop_header_icon=*/true);

  RemoveNonHeaderChildViews();
  views::ProgressBar* const progress_bar =
      AddChildView(std::make_unique<views::ProgressBar>());
  progress_bar->SetPreferredHeight(kProgressBarHeight);
  // Use an infinite animation: SetValue(-1).
  progress_bar->SetValue(-1);
  progress_bar->SetBackgroundColor(SK_ColorLTGRAY);
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kTopBottomPadding, kLeftRightPadding)));
  CHECK(!account->is_filtered_out);
  row->AddChildView(CreateAccountRow(account,
                                     /*clickable_position=*/std::nullopt,
                                     /*should_include_idp=*/false));
  AddChildView(std::move(row));

  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowSingleAccountConfirmDialog(
    const IdentityRequestAccountPtr& account,
    bool show_back_button) {
  std::u16string title = GetTitle(
      rp_data_, base::UTF8ToUTF16(account->identity_provider->idp_for_display),
      rp_context_);
  UpdateHeader(account->identity_provider->idp_metadata.brand_decoded_icon,
               title, webid::GetSubtitle(rp_data_), show_back_button,
               /*should_circle_crop_header_icon=*/true);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  std::pair<std::unique_ptr<views::View>, views::MdTextButton*>
      chooser_and_button = CreateSingleAccountChooser(account);
  AddChildView(std::move(chooser_and_button.first));

  // If the screen reader is active, request focus so that the creation of
  // this button is announced to the user. Do not do this when screen reader
  // is not active because it looks bad.
  if (ui::AXPlatform::GetInstance().IsScreenReaderActive()) {
    chooser_and_button.second->RequestFocus();
  }
  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowFailureDialog(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  UpdateHeader(idp_metadata.brand_decoded_icon,
               GetTitle(rp_data_, idp_for_display, rp_context_),
               webid::GetSubtitle(rp_data_),
               /*show_back_button=*/false,
               /*should_circle_crop_header_icon=*/true);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kLeftRightPadding)));

  // Add body.
  views::Label* const body = row->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_IDP_SIGNIN_STATUS_MISMATCH_DIALOG_BODY,
                                 idp_for_display),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  body->SetMultiLine(true);
  body->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  constexpr int kBodyLineHeight = 20;
  body->SetLineHeight(kBodyLineHeight);

  // Add space between the body and the separator and the body and the continue
  // button.
  constexpr int kBottomSpacing = 16;
  body->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kVerticalSpacing, 0, kBottomSpacing, 0)));

  // Add continue button.
  auto button = std::make_unique<ContinueButton>(
      base::BindRepeating(&FedCmAccountSelectionView::OnLoginToIdP,
                          base::Unretained(owner_), idp_metadata.config_url,
                          idp_metadata.idp_login_url),
      l10n_util::GetStringUTF16(IDS_SIGNIN_CONTINUE), idp_metadata,
      // TODO (kylixrd@): Shouldn't the following be a localizable string?
      /*extra_accessible_text=*/u"Opens in a new tab");
  row->AddChildView(std::move(button));
  AddChildView(std::move(row));

  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowErrorDialog(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  std::u16string title = GetTitle(rp_data_, idp_for_display, rp_context_);
  UpdateHeader(idp_metadata.brand_decoded_icon, title,
               webid::GetSubtitle(rp_data_),
               /*show_back_button=*/false,
               /*should_circle_crop_header_icon=*/true);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kTopBottomPadding, kLeftRightPadding)));

  std::u16string summary_text;
  std::u16string description_text;
  std::tie(summary_text, description_text) =
      GetErrorDialogText(error, idp_for_display);

  // Add error summary.
  views::Label* const summary =
      row->AddChildView(std::make_unique<views::Label>(
          summary_text, views::style::CONTEXT_DIALOG_TITLE,
          views::style::STYLE_PRIMARY));
  summary->SetMultiLine(true);
  summary->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  constexpr int kSummaryLineHeight = 22;
  summary->SetLineHeight(kSummaryLineHeight);

  // Add error description.
  views::Label* const description =
      row->AddChildView(std::make_unique<views::Label>(
          description_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  constexpr int kDescriptionLineHeight = 20;
  description->SetLineHeight(kDescriptionLineHeight);

  AddChildView(std::move(row));

  // Add row for buttons.
  auto button_row = std::make_unique<views::BoxLayoutView>();
  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  constexpr int kButtonRowTopPadding = 6;
  button_row->SetInsideBorderInsets(
      gfx::Insets::TLBR(kButtonRowTopPadding, 0, 0, kLeftRightPadding));
  constexpr int kButtonRowChildSpacing = 8;
  button_row->SetBetweenChildSpacing(kButtonRowChildSpacing);

  // Add more details button.
  if (error && !error->url.is_empty()) {
    auto more_details_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&FedCmAccountSelectionView::OnMoreDetails,
                            base::Unretained(owner_)),
        l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DIALOG_MORE_DETAILS_BUTTON));
    button_row->AddChildView(std::move(more_details_button));
  }

  // Add got it button.
  auto got_it_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&FedCmAccountSelectionView::OnGotIt,
                          base::Unretained(owner_)),
      l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DIALOG_GOT_IT_BUTTON));
  button_row->AddChildView(std::move(got_it_button));

  AddChildView(std::move(button_row));

  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowRequestPermissionDialog(
    const IdentityRequestAccountPtr& account) {
  NOTREACHED() << "ShowRequestPermissionDialog is only implemented for "
                  "AccountSelectionModalView";
}

std::string AccountSelectionBubbleView::GetDialogTitle() const {
  return base::UTF16ToUTF8(title_);
}

std::optional<std::string> AccountSelectionBubbleView::GetDialogSubtitle()
    const {
  if (subtitle_.empty()) {
    return std::nullopt;
  }
  return base::UTF16ToUTF8(subtitle_);
}

gfx::Rect AccountSelectionBubbleView::GetBubbleBounds(
    gfx::Rect proposed_bubble_bounds) {
  // Since the top right corner of the bubble is set as the arrow in the ctor,
  // the top right corner of the bubble will be anchored to the origin, which we
  // set to be the top right corner of the web contents container.
  //       |-------------------------|
  //       |                |        |
  //       |                | bubble |
  //       |                |        |
  //       |                |--------|
  //       |                         |
  //       |   contents_web_view     |
  //       |          ...            |
  //       |-------------------------|
  // We also need to inset the web contents bounds by kTopMargin at the top and
  // kRightMargin either at the left or right, depending on whether RTL is
  // enabled, in order to leave some space between the bubble and the edges of
  // the web contents.
  //       |-------------------------|
  //       |               kTopMargin|
  //       |         |--------|      |
  //       |         |        |kRight|
  //       |         | bubble |Margin|
  //       |         |--------|      |
  //       |                         |
  //       | contents_web_view       |
  //       |          ...            |
  //       |-------------------------|
  // In the RTL case, the bubble is aligned towards the left side of the screen
  // and the horizontal inset would apply to the left of the bubble.

  if (!owner_->web_contents()) {
    // Async autosize tasks may occur after the web_contents_ is destroyed.
    return proposed_bubble_bounds;
  }

  gfx::Rect web_contents_bounds = owner_->web_contents()->GetViewBounds();
  if (base::i18n::IsRTL()) {
    web_contents_bounds.Inset(gfx::Insets::TLBR(
        /*top=*/kTopMargin, /*left=*/kRightMargin, /*bottom=*/0,
        /*right=*/0));
    proposed_bubble_bounds.set_origin(
        owner_->web_contents()->GetViewBounds().origin());
  } else {
    web_contents_bounds.Inset(gfx::Insets::TLBR(
        /*top=*/kTopMargin, /*left=*/0, /*bottom=*/0,
        /*right=*/kRightMargin));
    proposed_bubble_bounds.set_origin(
        owner_->web_contents()->GetViewBounds().top_right());
  }
  proposed_bubble_bounds.AdjustToFit(web_contents_bounds);

  return proposed_bubble_bounds;
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateHeaderView() {
  auto header = std::make_unique<views::View>();
  // Do not use a top margin as it has already been set in the bubble.
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets::TLBR(
          0, kLeftRightPadding, kVerticalSpacing, kLeftRightPadding));

  // Add the space for the icon.
  auto image_view = std::make_unique<BrandIconImageView>(kBubbleIdpIconSize);
  image_view->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_right(kLeftRightPadding));
  header_icon_view_ = header->AddChildView(std::move(image_view));

  back_button_ =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&FedCmAccountSelectionView::OnBackButtonClicked,
                              base::Unretained(owner_)),
          vector_icons::kArrowBackIcon));
  views::InstallCircleHighlightPathGenerator(back_button_.get());
  back_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button_->SetVisible(false);

  int back_button_right_margin = kLeftRightPadding;
  // Set the right margin of the back button so that the back button and
  // the IDP brand icon have the same width. This ensures that the header
  // title does not shift when the user navigates to the consent screen.
  back_button_right_margin =
      std::max(0, back_button_right_margin +
                      header_icon_view_->GetPreferredSize().width() -
                      back_button_->GetPreferredSize().width());
  back_button_->SetProperty(views::kMarginsKey,
                            gfx::Insets().set_right(back_button_right_margin));

  auto* titles_container =
      header->AddChildView(std::make_unique<views::View>());
  titles_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  views::FlexSpecification flex_spec(views::LayoutOrientation::kHorizontal,
                                     views::MinimumFlexSizeRule::kScaleToZero,
                                     views::MaximumFlexSizeRule::kUnbounded);
  titles_container->SetProperty(views::kFlexBehaviorKey, flex_spec);

  // Add the title.
  title_label_ = titles_container->AddChildView(std::make_unique<views::Label>(
      title_, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  SetLabelProperties(title_label_);

  // Add the subtitle.
  subtitle_label_ =
      titles_container->AddChildView(std::make_unique<views::Label>(
          subtitle_, views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
  SetLabelProperties(subtitle_label_);
  subtitle_label_->SetVisible(!subtitle_.empty());

  // Add the close button.
  std::unique_ptr<views::Button> close_button =
      views::BubbleFrameView::CreateCloseButton(
          base::BindRepeating(&FedCmAccountSelectionView::OnCloseButtonClicked,
                              base::Unretained(owner_)));
  close_button->SetVisible(true);
  header->AddChildView(std::move(close_button));

  return header;
}

std::pair<std::unique_ptr<views::View>, views::MdTextButton*>
AccountSelectionBubbleView::CreateSingleAccountChooser(
    const IdentityRequestAccountPtr& account) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kLeftRightPadding), kVerticalSpacing));
  CHECK(!account->is_filtered_out);
  row->AddChildView(CreateAccountRow(account,
                                     /*clickable_position=*/std::nullopt,
                                     /*should_include_idp=*/false));

  // Prefer using the given name if it is provided, otherwise fallback to name,
  // unless that is disabled.
  std::u16string button_title = l10n_util::GetStringUTF16(IDS_SIGNIN_CONTINUE);
  if (!account->given_name.empty()) {
    button_title = l10n_util::GetStringFUTF16(
        IDS_ACCOUNT_SELECTION_CONTINUE, base::UTF8ToUTF16(account->given_name));
  }
  const content::IdentityProviderData& idp_data = *account->identity_provider;
  const content::IdentityProviderMetadata& idp_metadata = idp_data.idp_metadata;
  // We can pass crefs to OnAccountSelected because the `observer_` owns the
  // data.
  auto button = std::make_unique<ContinueButton>(
      base::BindRepeating(
          base::IgnoreResult(&FedCmAccountSelectionView::OnAccountSelected),
          base::Unretained(owner_), account),
      button_title, idp_metadata,
      base::UTF8ToUTF16(account->display_identifier));
  views::MdTextButton* button_ptr = button.get();
  row->AddChildView(std::move(button));

  // Do not add disclosure text if fields is empty.
  if (account->fields.empty()) {
    return std::make_pair(std::move(row), button_ptr);
  }

  // Add disclosure text.
  row->AddChildView(CreateDisclosureLabel(account));
  return std::make_pair(std::move(row), button_ptr);
}

void AccountSelectionBubbleView::AddSeparatorAndMultipleAccountChooser(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list) {
  auto scroll_view = std::make_unique<views::ScrollView>();

  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  views::View* const scroller_content =
      scroll_view->SetContents(std::make_unique<views::View>());
  scroller_content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  bool is_multi_idp = idp_list.size() > 1u;
  AddAccounts(accounts, scroller_content, is_multi_idp);
  size_t num_rows = accounts.size();
  std::optional<int> separator_size;
  // The size of the first button that prompts the user to login to IDP. This
  // may be either a "use a different account" due to filtered out accounts or a
  // mismatch login button.
  std::optional<int> first_login_button_size;

  // Add use other accounts due to filtered accounts or mismatches.
  for (const auto& idp_data : idp_list) {
    if (!idp_data->idp_metadata.has_filtered_out_account &&
        !idp_data->has_login_status_mismatch) {
      continue;
    }
    auto login_button =
        is_multi_idp
            ? CreateMultiIdpLoginRow(
                  base::UTF8ToUTF16(idp_data->idp_for_display), idp_data)
            : CreateSingleIdpUseOtherAccountButton(
                  idp_data->idp_metadata,
                  l10n_util::GetStringUTF16(
                      IDS_ACCOUNT_SELECTION_USE_OTHER_ACCOUNT),
                  kSingleIdpUseOtherAccountButtonIconMargin);
    if (accounts.size() > 0 && !first_login_button_size) {
      separator_size =
          AddLoginButtonSeparator(scroller_content, is_multi_idp, login_button);
      first_login_button_size = login_button->GetPreferredSize().height();
    }
    scroller_content->AddChildView(std::move(login_button));
    ++num_rows;
  }

  CHECK(num_rows > 0);
  bool starts_with_scroller = false;
  // The maximum height that the multi-account-picker can have. This value was
  // chosen so that if there are more than two accounts, the picker will show up
  // as a scrollbar showing 2 accounts plus half of the third one. Note that
  // this is an estimate if there are multiple IDPs, as IDP rows are not the
  // same height. That said, calling GetPreferredSize() is expensive so we are
  // ok with this estimate.
  float num_visible_rows = kNumVisibleRows;
  const int first_row_size =
      scroller_content->children()[0]->GetPreferredSize().height();
  int clipped_size = static_cast<int>(first_row_size * kNumVisibleRows);
  // When there are account rows but not enough to cover the visible rows, add
  // the mismatch size so that the scroller does not end awkwardly.
  if (0 < accounts.size() && accounts.size() < num_visible_rows &&
      first_login_button_size) {
    clipped_size += *separator_size + *first_login_button_size;
    ++num_visible_rows;
  }
  scroll_view->ClipHeightTo(0, clipped_size);
  if (num_rows > num_visible_rows) {
    starts_with_scroller = true;
  } else {
    // We will have some spacing between the scroller and the separator at the
    // top but we need some additional spacing to match the bottom margin,
    // which is slightly larger in single IDP case.
    scroll_view->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        is_multi_idp ? kVerticalSpacing - kTopBottomPadding : kVerticalSpacing,
        0, 0, 0)));
  }

  // We use a container for most of the contents here. If there is a scroller at
  // the start, we include the separator so that there is no spacing between the
  // separator and the scroller. And we also always include the accounts
  // followed by the IDP mismatches.
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  auto separator = std::make_unique<views::Separator>();
  if (!starts_with_scroller) {
    separator->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, 0, kTopBottomPadding, 0)));
  }
  container->AddChildView(std::move(separator));
  container->AddChildView(std::move(scroll_view));
  AddChildView(std::move(container));
}

void AccountSelectionBubbleView::AddAccounts(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    views::View* accounts_content,
    bool is_multi_idp) {
  int out_position = 0;
  if (!is_multi_idp) {
    for (const auto& account : accounts) {
      accounts_content->AddChildView(
          CreateAccountRow(account, /*clickable_position=*/out_position++,
                           /*should_include_idp=*/false));
    }
    return;
  }
  for (const auto& account : accounts) {
    // We notify the user that the account has been used in the past based on
    // the IdP's knowledge, e.g. `approved_clients` (or the browser knowledge if
    // that one is not present).
    std::optional<std::u16string> used_string =
        account->idp_claimed_login_state.value_or(
            account->browser_trusted_login_state) ==
                Account::LoginState::kSignIn
            ? std::make_optional<std::u16string>(
                  l10n_util::GetStringUTF16(IDS_USED_ON_THIS_SITE))
            : std::nullopt;
    accounts_content->AddChildView(
        CreateAccountRow(account, /*clickable_position=*/out_position++,
                         /*should_include_idp=*/true, /*is_modal_dialog=*/false,
                         /*additional_vertical_padding=*/0, used_string));
  }
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateMultiIdpLoginRow(
    const std::u16string& idp_for_display,
    const IdentityProviderDataPtr& idp_data) {
  auto image_view = std::make_unique<BrandIconImageView>(kMultiIdpIconSize);
  image_view->SetVisible(!idp_data->idp_metadata.brand_decoded_icon.IsEmpty());
  image_view->SetBrandIconImage(idp_data->idp_metadata.brand_decoded_icon,
                                /*should_circle_crop=*/true);

  auto button = std::make_unique<HoverButton>(
      base::BindRepeating(&FedCmAccountSelectionView::OnLoginToIdP,
                          base::Unretained(owner_),
                          idp_data->idp_metadata.config_url,
                          idp_data->idp_metadata.idp_login_url),
      std::move(image_view),
      l10n_util::GetStringFUTF16(
          IDS_ACCOUNT_SELECTION_USE_OTHER_ACCOUNT_MULTI_IDP, idp_for_display),
      /*subtitle=*/std::u16string(),
      /*secondary_view=*/
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kOpenInNewIcon, ui::kColorMenuIcon, kBubbleIdpIconSize)));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      /*vertical=*/kMultiIdpVerticalSpacing,
      /*horizontal=*/kLeftRightPadding)));
  button->SetIconHorizontalMargins(kMultiIdpIconLeftMargin,
                                   kMultiIdpIconRightMargin);
  button->AddExtraAccessibleText(
      l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_OPENS_IN_NEW_TAB));
  return button;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateSingleIdpUseOtherAccountButton(
    const content::IdentityProviderMetadata& idp_metadata,
    const std::u16string& title,
    int icon_margin) {
  auto icon_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kOpenInNewIcon, ui::kColorMenuIcon, kIdpLoginIconSize));
  auto button = std::make_unique<HoverButton>(
      base::BindRepeating(&FedCmAccountSelectionView::OnLoginToIdP,
                          base::Unretained(owner_), idp_metadata.config_url,
                          idp_metadata.idp_login_url),
      std::move(icon_view), title);
  button->SetIconHorizontalMargins(icon_margin, icon_margin);
  button->AddExtraAccessibleText(
      l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_OPENS_IN_NEW_TAB));
  return button;
}

void AccountSelectionBubbleView::UpdateHeader(
    const gfx::Image& idp_image,
    const std::u16string& title,
    const std::u16string& subtitle,
    bool show_back_button,
    bool should_circle_crop_header_icon) {
  back_button_->SetVisible(show_back_button);
  // The back button takes the place of the brand icon, if it is shown. By
  // default, we show placeholder brand icon prior to brand icon being fetched
  // so that header text wrapping does not change when brand icon is fetched.
  // Therefore, we need to hide the brand icon if the image is empty.
  if (show_back_button || idp_image.IsEmpty()) {
    header_icon_view_->SetVisible(false);
  } else {
    header_icon_view_->SetBrandIconImage(idp_image,
                                         should_circle_crop_header_icon);
    header_icon_view_->SetVisible(true);
  }
  if (title != title_) {
    title_ = title;
    title_label_->SetText(title_);
    // TODO(crbug.com/390581529): Make this work properly with subtitles.
    if (auto* widget = GetWidget()) {
      widget->widget_delegate()->SetAccessibleTitle(title_);
    }
    // The title label is not destroyed, so announce it manually.
    SendAccessibilityEvent(GetWidget(), title_);
  }
  if (subtitle != subtitle_) {
    subtitle_ = subtitle;
    subtitle_label_->SetText(subtitle_);
    subtitle_label_->SetVisible(!subtitle_.empty());
  }
}

void AccountSelectionBubbleView::RemoveNonHeaderChildViews() {
  const std::vector<raw_ptr<views::View, VectorExperimental>> child_views =
      children();
  for (views::View* child_view : child_views) {
    if (child_view != header_view_) {
      RemoveChildViewT(child_view);
    }
  }
}

BEGIN_METADATA(AccountSelectionBubbleView)
END_METADATA

}  // namespace webid
