// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include "base/i18n/case_conversion.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
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
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// The radius used for the corner of the "Continue as" button.
constexpr int kButtonRadius = 16;
// The fixed, total width of the bubble.
constexpr int kBubbleWidth = 375;
// The desired size of the avatars of user accounts.
constexpr int kDesiredAvatarSize = 30;
// The desired size of the icon of the identity provider.
constexpr int kDesiredIdpIconSize = 20;
// The size of the padding used at the top and bottom of the bubble.
constexpr int kTopBottomPadding = 4;
// The size of the horizontal padding between the bubble content and the edge of
// the bubble, as well as the horizontal padding between icons and text.
constexpr int kLeftRightPadding = 12;
// The size of the vertical padding for most elements in the bubble.
constexpr int kVerticalSpacing = 8;
// The height of the progress bar shown when showing "Verifying...".
constexpr int kProgressBarHeight = 2;
// The size of the space between the right boundary of the WebContents and the
// right boundary of the bubble.
constexpr int kRightMargin = 40;
// The size of the space between the top boundary of the WebContents and the top
// boundary of the bubble.
constexpr int kTopMargin = 16;

constexpr char kImageFetcherUmaClient[] = "FedCMAccountChooser";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("fedcm_account_profile_image_fetcher",
                                        R"(
        semantics {
          sender: "Profile image fetcher for FedCM Account chooser on desktop."
          description:
            "Retrieves profile images for user's accounts in the FedCM login"
            "flow."
          trigger:
            "Triggered when FedCM API is called and account chooser shows up."
            "The accounts shown are ones for which the user has previously"
            "signed into the identity provider."
          data:
            "Account picture URL of user account, provided by the identity"
            "provider."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature in chrome://settings, under"
            "'Privacy and security', then 'Site Settings', and finally"
            "'Third party sign-in'."
          policy_exception_justification:
            "Not implemented. This is a feature that sites use for"
            "Federated Sign-In, for which we do not have an Enterprise policy."
        })");

// A CanvasImageSource that fills a gray circle with a monogram.
class LetterCircleCroppedImageSkiaSource : public gfx::CanvasImageSource {
 public:
  LetterCircleCroppedImageSkiaSource(const std::u16string& letter, int size)
      : gfx::CanvasImageSource(gfx::Size(size, size)), letter_(letter) {}

  LetterCircleCroppedImageSkiaSource(
      const LetterCircleCroppedImageSkiaSource&) = delete;
  LetterCircleCroppedImageSkiaSource& operator=(
      const LetterCircleCroppedImageSkiaSource&) = delete;
  ~LetterCircleCroppedImageSkiaSource() override = default;

  void Draw(gfx::Canvas* canvas) override {
    monogram::DrawMonogramInCanvas(canvas, size().width(), size().width(),
                                   letter_, SK_ColorWHITE, SK_ColorGRAY);
  }

 private:
  const std::u16string letter_;
};

// A CanvasImageSource that:
// 1) Applies an optional square center-crop.
// 2) Resizes the cropped image (while maintaining the image's aspect ratio) to
//    fit into the target canvas. If no center-crop was applied and the source
//    image is rectangular, the image is resized so that
//    `avatar` small edge size == `canvas_edge_size`.
// 3) Circle center-crops the resized image.
class CircleCroppedImageSkiaSource : public gfx::CanvasImageSource {
 public:
  CircleCroppedImageSkiaSource(gfx::ImageSkia avatar,
                               absl::optional<int> pre_resize_avatar_crop_size,
                               int canvas_edge_size)
      : gfx::CanvasImageSource(gfx::Size(canvas_edge_size, canvas_edge_size)) {
    int scaled_width = canvas_edge_size;
    int scaled_height = canvas_edge_size;
    if (pre_resize_avatar_crop_size) {
      float avatar_scale =
          (canvas_edge_size / (float)*pre_resize_avatar_crop_size);
      scaled_width = floor(avatar.width() * avatar_scale);
      scaled_height = floor(avatar.height() * avatar_scale);
    } else {
      // Resize `avatar` so that it completely fills the canvas.
      float height_ratio = ((float)avatar.height() / (float)avatar.width());
      if (height_ratio >= 1.0f)
        scaled_height = floor(canvas_edge_size * height_ratio);
      else
        scaled_width = floor(canvas_edge_size / height_ratio);
    }
    avatar_ = gfx::ImageSkiaOperations::CreateResizedImage(
        avatar, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(scaled_width, scaled_height));
  }

  CircleCroppedImageSkiaSource(const CircleCroppedImageSkiaSource&) = delete;
  CircleCroppedImageSkiaSource& operator=(const CircleCroppedImageSkiaSource&) =
      delete;
  ~CircleCroppedImageSkiaSource() override = default;

  // CanvasImageSource override:
  void Draw(gfx::Canvas* canvas) override {
    int canvas_edge_size = size().width();

    // Center the avatar in the canvas.
    int x = (canvas_edge_size - avatar_.width()) / 2;
    int y = (canvas_edge_size - avatar_.height()) / 2;

    SkPath circular_mask;
    circular_mask.addCircle(SkIntToScalar(canvas_edge_size / 2),
                            SkIntToScalar(canvas_edge_size / 2),
                            SkIntToScalar(canvas_edge_size / 2));
    canvas->ClipPath(circular_mask, true);
    canvas->DrawImageInt(avatar_, x, y);
  }

 private:
  gfx::ImageSkia avatar_;
};

// views::MdTextButton which:
// - Uses the passed-in `brand_background_color` based on whether the button
//   background contrasts sufficiently with dialog background.
// - If `brand_text_color` is not provided, computes the text color such that it
//   contrasts sufficiently with `brand_background_color`.
class ContinueButton : public views::MdTextButton {
 public:
  ContinueButton(views::MdTextButton::PressedCallback callback,
                 const std::u16string& text,
                 AccountSelectionBubbleView* bubble_view,
                 absl::optional<SkColor> brand_background_color,
                 absl::optional<SkColor> brand_text_color)
      : views::MdTextButton(callback, text),
        bubble_view_(bubble_view),
        brand_background_color_(brand_background_color),
        brand_text_color_(brand_text_color) {}

  ContinueButton(const ContinueButton&) = delete;
  ContinueButton& operator=(const ContinueButton&) = delete;
  ~ContinueButton() override = default;

  void OnThemeChanged() override {
    views::MdTextButton::OnThemeChanged();
    if (!brand_background_color_)
      return;

    SkColor dialog_background_color = bubble_view_->GetBackgroundColor();
    if (color_utils::GetContrastRatio(dialog_background_color,
                                      *brand_background_color_) <
        color_utils::kMinimumReadableContrastRatio) {
      SetBgColorOverride(absl::nullopt);
      SetEnabledTextColors(absl::nullopt);
      return;
    }
    SetBgColorOverride(*brand_background_color_);
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
  base::raw_ptr<AccountSelectionBubbleView> bubble_view_;
  absl::optional<SkColor> brand_background_color_;
  absl::optional<SkColor> brand_text_color_;
};

class AccountImageView : public views::ImageView {
 public:
  AccountImageView() = default;

  AccountImageView(const AccountImageView&) = delete;
  AccountImageView& operator=(const AccountImageView&) = delete;
  ~AccountImageView() override = default;

  // Fetch image and set it on AccountImageView.
  void FetchImage(const content::IdentityRequestAccount& account,
                  image_fetcher::ImageFetcher& image_fetcher) {
    image_fetcher::ImageFetcherParams params(kTrafficAnnotation,
                                             kImageFetcherUmaClient);

    // OnImageFetched() is a member of AccountImageView so that the callback
    // is cancelled in the case that AccountImageView is destroyed prior to
    // the callback returning.
    image_fetcher.FetchImage(account.picture,
                             base::BindOnce(&AccountImageView::OnImageFetched,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            base::UTF8ToUTF16(account.name)),
                             std::move(params));
  }

 private:
  void OnImageFetched(const std::u16string& account_name,
                      const gfx::Image& image,
                      const image_fetcher::RequestMetadata& metadata) {
    gfx::ImageSkia avatar;
    if (image.IsEmpty()) {
      std::u16string letter = account_name;
      if (letter.length() > 0)
        letter = base::i18n::ToUpper(account_name.substr(0, 1));
      avatar = gfx::CanvasImageSource::MakeImageSkia<
          LetterCircleCroppedImageSkiaSource>(letter, kDesiredAvatarSize);
    } else {
      avatar =
          gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
              image.AsImageSkia(), absl::nullopt, kDesiredAvatarSize);
    }
    SetImage(avatar);
  }

  base::WeakPtrFactory<AccountImageView> weak_ptr_factory_{this};
};

void SendAccessibilityEvent(views::Widget* widget,
                            std::u16string announcement) {
  if (!widget)
    return;

  views::View* root_view = widget->GetRootView();
#if BUILDFLAG(IS_MAC)
  if (!announcement.empty())
    root_view->GetViewAccessibility().OverrideName(announcement);
  root_view->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
#else
  if (!announcement.empty())
    root_view->GetViewAccessibility().AnnounceText(announcement);
#endif
}

}  // namespace

AccountSelectionBubbleView::AccountSelectionBubbleView(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    base::span<const content::IdentityRequestAccount> accounts,
    const content::IdentityProviderMetadata& idp_metadata,
    const content::ClientIdData& client_data,
    views::View* anchor_view,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TabStripModel* tab_strip_model,
    base::OnceCallback<void(const content::IdentityRequestAccount&)>
        on_account_selected_callback)
    : views::BubbleDialogDelegateView(
          anchor_view,
          // Note that BOTTOM_RIGHT means the bubble's bottom and right are
          // anchored to the `anchor_view`, which effectively means the bubble
          // will be on top of the `anchor_view`, aligned on its right side.
          views::BubbleBorder::Arrow::BOTTOM_RIGHT),
      idp_for_display_(base::UTF8ToUTF16(idp_for_display)),
      brand_text_color_(idp_metadata.brand_text_color),
      brand_background_color_(idp_metadata.brand_background_color),
      client_data_(client_data),
      account_list_(accounts.begin(), accounts.end()),
      tab_strip_model_(tab_strip_model),
      on_account_selected_callback_(std::move(on_account_selected_callback)) {
  image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(), url_loader_factory);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(kBubbleWidth);
  set_margins(gfx::Insets::VH(kTopBottomPadding + kVerticalSpacing, 0));
  // TODO(crbug.com/1323298): we are currently using a custom header because the
  // icon, title, and close buttons from a bubble are not customizable enough to
  // satisfy the UI requirements. However, this adds complexity to the code and
  // makes this bubble lose any improvements made to the base bubble, so we
  // should revisit this.
  SetShowTitle(false);
  SetShowCloseButton(false);
  set_close_on_deactivate(false);

  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT,
      base::UTF8ToUTF16(rp_for_display), idp_for_display_);
  SetAccessibleTitle(title);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kTopBottomPadding));
  bool has_icon = idp_metadata.brand_icon_url.is_valid();
  header_view_ = AddChildView(CreateHeaderView(title, has_icon));
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateAccountChooser(accounts));

  if (has_icon) {
    image_fetcher::ImageFetcherParams params(kTrafficAnnotation,
                                             kImageFetcherUmaClient);
    image_fetcher_->FetchImage(
        idp_metadata.brand_icon_url,
        base::BindOnce(&AccountSelectionBubbleView::OnBrandImageFetched,
                       weak_ptr_factory_.GetWeakPtr()),
        std::move(params));
  }
}

AccountSelectionBubbleView::~AccountSelectionBubbleView() = default;

gfx::Rect AccountSelectionBubbleView::GetBubbleBounds() {
  // The bubble initially looks like this relative to the contents_web_view:
  //                        |--------|
  //                        |        |
  //                        | bubble |
  //                        |        |
  //       |-------------------------|
  //       |                         |
  //       | contents_web_view       |
  //       |          ...            |
  //       |-------------------------|
  // Thus, we need to move the bubble to the left by kRightMargin and down by
  // the size of the bubble plus kTopMargin in order to achieve what we want:
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
  return views::BubbleDialogDelegateView::GetBubbleBounds() +
         gfx::Vector2d(-kRightMargin,
                       GetWidget()->client_view()->GetPreferredSize().height() +
                           kTopMargin);
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateHeaderView(
    const std::u16string& title,
    bool has_icon) {
  auto header = std::make_unique<views::View>();
  // Do not use a top margin as it has already been set in the bubble.
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets::TLBR(
          0, kLeftRightPadding, kVerticalSpacing, kLeftRightPadding));

  // Add the icon.
  if (has_icon) {
    // Show placeholder brand icon prior to brand icon being fetched so that
    // header text wrapping does not change when brand icon is fetched.
    auto image_view = std::make_unique<views::ImageView>();
    image_view->SetImageSize(
        gfx::Size(kDesiredIdpIconSize, kDesiredIdpIconSize));
    image_view->SetProperty(views::kMarginsKey,
                            gfx::Insets().set_right(kLeftRightPadding));
    header_icon_view_ = header->AddChildView(image_view.release());
  }

  back_button_ =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&AccountSelectionBubbleView::HandleBackPressed,
                              base::Unretained(this)),
          vector_icons::kArrowBackIcon));
  views::InstallCircleHighlightPathGenerator(back_button_.get());
  back_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button_->SetVisible(false);

  int back_button_right_margin = kLeftRightPadding;
  if (header_icon_view_) {
    // Set the right margin of the back button so that the back button and
    // the IDP brand icon have the same width. This ensures that the header
    // title does not shift when the user navigates to the consent screen.
    back_button_right_margin =
        std::max(0, back_button_right_margin +
                        header_icon_view_->GetPreferredSize().width() -
                        back_button_->GetPreferredSize().width());
  }
  back_button_->SetProperty(views::kMarginsKey,
                            gfx::Insets().set_right(back_button_right_margin));

  // Add the title.
  title_label_ = header->AddChildView(std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  title_label_->SetMultiLine(true);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAllowCharacterBreak(true);
  title_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true));

  // Add the close button.
  std::unique_ptr<views::Button> close_button =
      views::BubbleFrameView::CreateCloseButton(
          base::BindRepeating(&AccountSelectionBubbleView::CloseBubble,
                              weak_ptr_factory_.GetWeakPtr()));
  close_button->SetVisible(true);
  header->AddChildView(close_button.release());
  return header;
}

void AccountSelectionBubbleView::CloseBubble() {
  if (!GetWidget())
    return;
  UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.CloseVerifySheet.Desktop",
                        verify_sheet_shown_);
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateAccountChooser(
    base::span<const content::IdentityRequestAccount> accounts) {
  DCHECK(!accounts.empty());
  if (accounts.size() == 1u) {
    return CreateSingleAccountChooser(accounts.front());
  }
  return CreateMultipleAccountChooser(accounts);
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateSingleAccountChooser(
    const content::IdentityRequestAccount& account) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kLeftRightPadding), kVerticalSpacing));
  row->AddChildView(CreateAccountRow(account, /*should_hover=*/false));

  // Prefer using the given name if it is provided, otherwise fallback to name.
  std::string display_name =
      account.given_name.empty() ? account.name : account.given_name;
  auto button = std::make_unique<ContinueButton>(
      base::BindRepeating(&AccountSelectionBubbleView::OnClickedContinue,
                          weak_ptr_factory_.GetWeakPtr(), account),
      l10n_util::GetStringFUTF16(IDS_ACCOUNT_SELECTION_CONTINUE,
                                 base::UTF8ToUTF16(display_name)),
      this, brand_background_color_, brand_text_color_);
  button->SetCornerRadius(kButtonRadius);
  button->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  button->SetProminent(true);
  continue_button_ = row->AddChildView(std::move(button));

  // Do not add disclosure text if this is a sign in.
  if (account.login_state == Account::LoginState::kSignIn)
    return row;

  // Add disclosure text. It requires a StyledLabel so that we can add the links
  // to the privacy policy and terms of service URLs.
  views::StyledLabel* disclosure_label =
      row->AddChildView(std::make_unique<views::StyledLabel>());
  // TODO(crbug.com/1324689): remove the IsRTL() check and instead replace with
  // just gfx::HorizontalAlignment::ALIGN_LEFT when
  // StyledLabel::SetHorizontalAlignment() does mirror in RTL.
  disclosure_label->SetHorizontalAlignment(
      base::i18n::IsRTL() ? gfx::HorizontalAlignment::ALIGN_RIGHT
                          : gfx::HorizontalAlignment::ALIGN_LEFT);

  // Set custom top margin for `disclosure_label` in order to take
  // (line_height - font_height) into account.
  disclosure_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(5, 0, 0, 0)));
  disclosure_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);

  std::vector<size_t> offsets;

  if (client_data_.privacy_policy_url.is_empty() &&
      client_data_.terms_of_service_url.is_empty()) {
    // Case for both the privacy policy and terms of service URLs are missing.
    std::u16string disclosure_text = l10n_util::GetStringFUTF16(
        IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_PP_OR_TOS,
        {idp_for_display_});
    disclosure_label->SetText(disclosure_text);
    return row;
  }

  if (client_data_.privacy_policy_url.is_empty()) {
    // Case for when we only need to add a link for terms of service URL, but
    // not privacy policy. We use two placeholders for the start and end of
    // 'terms of service' in order to style that text as a link.
    std::u16string disclosure_text = l10n_util::GetStringFUTF16(
        IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_PP,
        {idp_for_display_, std::u16string(), std::u16string()}, &offsets);
    disclosure_label->SetText(disclosure_text);
    // Add link styling for terms of service url.
    disclosure_label->AddStyleRange(
        gfx::Range(offsets[1], offsets[2]),
        views::StyledLabel::RangeStyleInfo::CreateForLink(
            base::BindRepeating(&AccountSelectionBubbleView::OnLinkClicked,
                                weak_ptr_factory_.GetWeakPtr(),
                                client_data_.terms_of_service_url)));
    return row;
  }

  if (client_data_.terms_of_service_url.is_empty()) {
    // Case for when we only need to add a link for privacy policy URL, but not
    // terms of service. We use two placeholders for the start and end of
    // 'privacy policy' in order to style that text as a link.
    std::u16string disclosure_text = l10n_util::GetStringFUTF16(
        IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_TOS,
        {idp_for_display_, std::u16string(), std::u16string()}, &offsets);
    disclosure_label->SetText(disclosure_text);
    // Add link styling for privacy policy url.
    disclosure_label->AddStyleRange(
        gfx::Range(offsets[1], offsets[2]),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &AccountSelectionBubbleView::OnLinkClicked,
            weak_ptr_factory_.GetWeakPtr(), client_data_.privacy_policy_url)));
    return row;
  }

  // Case for when we add a link for privacy policy URL as well as
  // terms of service URL. We use four placeholders at start/end of both
  // 'privacy policy' and 'terms of service' in order to style both of them as
  // links.
  std::vector<std::u16string> replacements = {
      idp_for_display_, std::u16string(), std::u16string(), std::u16string(),
      std::u16string()};
  std::u16string disclosure_text = l10n_util::GetStringFUTF16(
      IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT, replacements, &offsets);
  disclosure_label->SetText(disclosure_text);
  // Add link styling for privacy policy url.
  disclosure_label->AddStyleRange(
      gfx::Range(offsets[1], offsets[2]),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &AccountSelectionBubbleView::OnLinkClicked,
          weak_ptr_factory_.GetWeakPtr(), client_data_.privacy_policy_url)));
  // Add link styling for terms of service url.
  disclosure_label->AddStyleRange(
      gfx::Range(offsets[3], offsets[4]),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &AccountSelectionBubbleView::OnLinkClicked,
          weak_ptr_factory_.GetWeakPtr(), client_data_.terms_of_service_url)));
  return row;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateMultipleAccountChooser(
    base::span<const content::IdentityRequestAccount> accounts) {
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  views::View* row = scroll_view->SetContents(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  for (const auto& account : accounts) {
    row->AddChildView(CreateAccountRow(account, /*should_hover=*/true));
  }
  // The maximum height that the multi-account-picker can have. This value was
  // chosen so that if there are more than two accounts, the picker will show up
  // as a scrollbar showing 2 accounts plus half of the third one.
  int per_account_size = row->GetPreferredSize().height() / accounts.size();
  scroll_view->ClipHeightTo(0, static_cast<int>(per_account_size * 2.5));
  return scroll_view;
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateAccountRow(
    const content::IdentityRequestAccount& account,
    bool should_hover) {
  auto image_view = std::make_unique<AccountImageView>();
  image_view->SetImageSize({kDesiredAvatarSize, kDesiredAvatarSize});
  image_view->FetchImage(account, *image_fetcher_);
  if (should_hover) {
    auto row = std::make_unique<HoverButton>(
        base::BindRepeating(&AccountSelectionBubbleView::OnSingleAccountPicked,
                            weak_ptr_factory_.GetWeakPtr(), account),
        std::move(image_view), base::UTF8ToUTF16(account.name),
        base::UTF8ToUTF16(account.email));
    row->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(/*vertical=*/0, /*horizontal=*/kLeftRightPadding)));
    row->SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                              views::style::STYLE_SECONDARY);
    return row;
  }
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(/*vertical=*/kVerticalSpacing, /*horizontal=*/0),
      kLeftRightPadding));
  row->AddChildView(std::move(image_view));
  views::View* text_column = row->AddChildView(std::make_unique<views::View>());
  text_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Add account name.
  views::Label* account_name =
      text_column->AddChildView(std::make_unique<views::Label>(
          base::UTF8ToUTF16(account.name),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  account_name->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Add account email.
  views::Label* account_email = text_column->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(account.email),
                                     views::style::CONTEXT_DIALOG_BODY_TEXT,
                                     views::style::STYLE_SECONDARY));
  account_email->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  return row;
}

void AccountSelectionBubbleView::OnBrandImageFetched(
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (header_icon_view_ != nullptr && image.Width() == image.Height() &&
      image.Width() >= AccountSelectionView::GetBrandIconMinimumSize()) {
    gfx::ImageSkia resized_image =
        gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
            image.AsImageSkia(),
            image.Width() *
                FedCmAccountSelectionView::kMaskableWebIconSafeZoneRatio,
            kDesiredIdpIconSize);
    header_icon_view_->SetImage(resized_image);
  }
}

void AccountSelectionBubbleView::OnLinkClicked(const GURL& gurl) {
  DCHECK(tab_strip_model_);
  // Add a tab for the URL at the end of the tab strip, in the foreground.
  tab_strip_model_->delegate()->AddTabAt(gurl, -1, true);
}

void AccountSelectionBubbleView::OnSingleAccountPicked(
    const content::IdentityRequestAccount& account) {
  if (account.login_state == Account::LoginState::kSignIn) {
    OnClickedContinue(account);
    return;
  }
  RemoveNonHeaderChildViews();
  SetBackButtonVisible(true);
  AddChildView(std::make_unique<views::Separator>());
  std::vector<content::IdentityRequestAccount> accounts = {account};
  AddChildView(CreateAccountChooser(accounts));
  SizeToContents();
  PreferredSizeChanged();

  // Focusing `continue_button_` without screen reader on makes the UI look
  // awkward, so we only want to do so when screen reader is enabled.
  if (accessibility_state_utils::IsScreenReaderEnabled())
    continue_button_->RequestFocus();
  SendAccessibilityEvent(GetWidget(), std::u16string());
}

void AccountSelectionBubbleView::HandleBackPressed() {
  RemoveNonHeaderChildViews();
  SetBackButtonVisible(false);
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateAccountChooser(account_list_));
  SizeToContents();
  PreferredSizeChanged();
}

void AccountSelectionBubbleView::OnClickedContinue(
    const content::IdentityRequestAccount& account) {
  ShowVerifySheet(account);
  std::move(on_account_selected_callback_).Run(account);
}

void AccountSelectionBubbleView::ShowVerifySheet(
    const content::IdentityRequestAccount& account) {
  verify_sheet_shown_ = true;
  RemoveNonHeaderChildViews();
  SetBackButtonVisible(false);
  std::u16string title = l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE);
  title_label_->SetText(title);
  views::ProgressBar* progress_bar =
      AddChildView(std::make_unique<views::ProgressBar>(kProgressBarHeight));
  // Use an infinite animation: SetValue(-1).
  progress_bar->SetValue(-1);
  progress_bar->SetBackgroundColor(SK_ColorLTGRAY);
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kTopBottomPadding, kLeftRightPadding)));
  row->AddChildView(CreateAccountRow(account, /*should_hover=*/false));
  AddChildView(row.release());
  SizeToContents();
  PreferredSizeChanged();

  SendAccessibilityEvent(GetWidget(), title);
}

void AccountSelectionBubbleView::SetBackButtonVisible(bool is_visible) {
  back_button_->SetVisible(is_visible);
  if (header_icon_view_)
    header_icon_view_->SetVisible(!is_visible);
}

void AccountSelectionBubbleView::RemoveNonHeaderChildViews() {
  std::vector<views::View*> child_views = children();
  for (views::View* child_view : child_views) {
    if (child_view != header_view_) {
      RemoveChildView(child_view);
      delete child_view;
    }
  }
}

BEGIN_METADATA(AccountSelectionBubbleView, views::BubbleDialogDelegateView)
END_METADATA
