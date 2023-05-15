// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_features.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
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
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
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
      const float avatar_scale =
          (canvas_edge_size / (float)*pre_resize_avatar_crop_size);
      scaled_width = floor(avatar.width() * avatar_scale);
      scaled_height = floor(avatar.height() * avatar_scale);
    } else {
      // Resize `avatar` so that it completely fills the canvas.
      const float height_ratio =
          ((float)avatar.height() / (float)avatar.width());
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
    const int canvas_edge_size = size().width();

    // Center the avatar in the canvas.
    const int x = (canvas_edge_size - avatar_.width()) / 2;
    const int y = (canvas_edge_size - avatar_.height()) / 2;

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
                 const content::IdentityProviderMetadata& idp_metadata)
      : views::MdTextButton(callback, text),
        bubble_view_(bubble_view),
        brand_background_color_(idp_metadata.brand_background_color),
        brand_text_color_(idp_metadata.brand_text_color) {
    SetCornerRadius(kButtonRadius);
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetProminent(true);
  }

  ContinueButton(const ContinueButton&) = delete;
  ContinueButton& operator=(const ContinueButton&) = delete;
  ~ContinueButton() override = default;

  void OnThemeChanged() override {
    views::MdTextButton::OnThemeChanged();
    if (!brand_background_color_)
      return;

    const SkColor dialog_background_color = bubble_view_->GetBackgroundColor();
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
  raw_ptr<AccountSelectionBubbleView> bubble_view_;
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

// Wrapper around ImageViews for IDP icons. Used to ensure that the fetch
// callback is not run when the ImageView has been deleted.
class IdpImageView : public views::ImageView {
 public:
  explicit IdpImageView(AccountSelectionBubbleView* bubble_view)
      : bubble_view_(bubble_view) {}

  IdpImageView(const IdpImageView&) = delete;
  IdpImageView& operator=(const IdpImageView&) = delete;
  ~IdpImageView() override = default;

  // Fetch image and set it on IdpImageView.
  void FetchImage(const GURL& icon_url,
                  image_fetcher::ImageFetcher& image_fetcher) {
    image_fetcher::ImageFetcherParams params(kTrafficAnnotation,
                                             kImageFetcherUmaClient);
    image_fetcher.FetchImage(
        icon_url,
        base::BindOnce(&IdpImageView::OnImageFetched,
                       weak_ptr_factory_.GetWeakPtr(), icon_url),
        std::move(params));
  }

 private:
  void OnImageFetched(const GURL& image_url,
                      const gfx::Image& image,
                      const image_fetcher::RequestMetadata& metadata) {
    if (image.Width() != image.Height() ||
        image.Width() < AccountSelectionView::GetBrandIconMinimumSize()) {
      return;
    }
    gfx::ImageSkia idp_image =
        gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
            image.AsImageSkia(),
            image.Width() *
                FedCmAccountSelectionView::kMaskableWebIconSafeZoneRatio,
            kDesiredIdpIconSize);
    SetImage(idp_image);
    bubble_view_->AddIdpImage(image_url, idp_image);
  }

  // The AccountSelectionBubbleView outlives IdpImageView so it is safe to store
  // a raw pointer to it.
  raw_ptr<AccountSelectionBubbleView> bubble_view_;

  base::WeakPtrFactory<IdpImageView> weak_ptr_factory_{this};
};

void SendAccessibilityEvent(views::Widget* widget,
                            std::u16string announcement) {
  if (!widget)
    return;

  views::View* const root_view = widget->GetRootView();
#if BUILDFLAG(IS_MAC)
  if (!announcement.empty())
    root_view->GetViewAccessibility().OverrideName(announcement);
  root_view->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
#else
  if (!announcement.empty())
    root_view->GetViewAccessibility().AnnounceText(announcement);
#endif
}

// Selects string for disclosure text based on passed-in `privacy_policy_url`
// and `terms_of_service_url`.
int SelectDisclosureTextResourceId(const GURL& privacy_policy_url,
                                   const GURL& terms_of_service_url) {
  if (privacy_policy_url.is_empty()) {
    return terms_of_service_url.is_empty()
               ? IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_PP_OR_TOS
               : IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_PP;
  }

  return terms_of_service_url.is_empty()
             ? IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_TOS
             : IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT;
}

int SelectSingleIdpTitleResourceId(blink::mojom::RpContext rp_context) {
  switch (rp_context) {
    case blink::mojom::RpContext::kSignIn:
      return IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT_SIGN_IN;
    case blink::mojom::RpContext::kSignUp:
      return IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT_SIGN_UP;
    case blink::mojom::RpContext::kUse:
      return IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT_USE;
    case blink::mojom::RpContext::kContinue:
      return IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT_CONTINUE;
  }
}

void SetTitleHeaderProperties(views::Label* label) {
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true));
}

// Returns the title to be shown in the dialog. This does not include the
// subtitle. For screen reader purposes, GetAccessibleTitle() is used instead.
std::u16string GetTitle(
    const std::u16string& top_frame_for_display,
    const absl::optional<std::u16string>& iframe_for_display,
    const absl::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context) {
  std::u16string frame_in_title = iframe_for_display.has_value()
                                      ? iframe_for_display.value()
                                      : top_frame_for_display;
  return idp_title.has_value()
             ? l10n_util::GetStringFUTF16(
                   SelectSingleIdpTitleResourceId(rp_context), frame_in_title,
                   idp_title.value())
             : l10n_util::GetStringFUTF16(
                   IDS_MULTI_IDP_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT,
                   frame_in_title);
}

std::u16string GetSubtitle(const std::u16string& top_frame_for_display) {
  return l10n_util::GetStringFUTF16(IDS_ACCOUNT_SELECTION_SHEET_SUBTITLE,
                                    top_frame_for_display);
}

// Returns the title combined with the subtitle for screen reader purposes.
std::u16string GetAccessibleTitle(
    const std::u16string& top_frame_for_display,
    const absl::optional<std::u16string>& iframe_for_display,
    const absl::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context) {
  std::u16string title = GetTitle(top_frame_for_display, iframe_for_display,
                                  idp_title, rp_context);
  return iframe_for_display.has_value()
             ? title + u" " + GetSubtitle(top_frame_for_display)
             : title;
}

}  // namespace

AccountSelectionBubbleView::AccountSelectionBubbleView(
    const std::u16string& top_frame_for_display,
    const absl::optional<std::u16string>& iframe_for_display,
    const absl::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    bool show_auto_reauthn_checkbox,

    views::View* anchor_view,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Observer* observer)
    : views::BubbleDialogDelegateView(
          anchor_view,
          // Note that BOTTOM_RIGHT means the bubble's bottom and right are
          // anchored to the `anchor_view`, which effectively means the bubble
          // will be on top of the `anchor_view`, aligned on its right side.
          views::BubbleBorder::Arrow::BOTTOM_RIGHT),
      observer_(observer) {
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

  // If `idp_title` is absl::nullopt, we are going to show multi-IDP UI. DCHECK
  // that we do not get to this when the flag is disabled.
  DCHECK(
      idp_title.has_value() ||
      base::FeatureList::IsEnabled(features::kFedCmMultipleIdentityProviders));

  rp_context_ = rp_context;
  show_auto_reauthn_checkbox_ = show_auto_reauthn_checkbox;
  title_ = GetTitle(top_frame_for_display, iframe_for_display, idp_title,
                    rp_context);
  accessible_title_ = GetAccessibleTitle(
      top_frame_for_display, iframe_for_display, idp_title, rp_context);
  SetAccessibleTitle(accessible_title_);

  if (iframe_for_display.has_value()) {
    subtitle_ = ::GetSubtitle(top_frame_for_display);
  }

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kTopBottomPadding));
  header_view_ =
      AddChildView(CreateHeaderView(/*has_idp_icon=*/idp_title.has_value()));
}

AccountSelectionBubbleView::~AccountSelectionBubbleView() = default;

void AccountSelectionBubbleView::ShowMultiAccountPicker(
    const std::vector<IdentityProviderDisplayData>& idp_display_data_list) {
  // If there are multiple IDPs, then the content::IdentityProviderMetadata
  // passed will be unused since there will be no `header_icon_view_`.
  // Therefore, it is fine to pass the first one into UpdateHeader().
  DCHECK(idp_display_data_list.size() == 1u || !header_icon_view_);
  UpdateHeader(idp_display_data_list[0].idp_metadata, title_, subtitle_,
               /*show_back_button=*/false);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateMultipleAccountChooser(idp_display_data_list));
  SizeToContents();
  PreferredSizeChanged();

  if (has_sheet_) {
    // Focusing `continue_button_` without screen reader on makes the UI look
    // awkward, so we only want to do so when screen reader is enabled.
    if (accessibility_state_utils::IsScreenReaderEnabled() && continue_button_)
      continue_button_->RequestFocus();
    SendAccessibilityEvent(GetWidget(), std::u16string());
  }

  has_sheet_ = true;
}

void AccountSelectionBubbleView::ShowVerifyingSheet(
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    const std::u16string& title) {
  UpdateHeader(idp_display_data.idp_metadata, title,
               /*subpage_subtitle=*/u"", /*show_back_button=*/false);

  RemoveNonHeaderChildViews();
  views::ProgressBar* const progress_bar =
      AddChildView(std::make_unique<views::ProgressBar>(kProgressBarHeight));
  // Use an infinite animation: SetValue(-1).
  progress_bar->SetValue(-1);
  progress_bar->SetBackgroundColor(SK_ColorLTGRAY);
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kTopBottomPadding, kLeftRightPadding)));
  row->AddChildView(
      CreateAccountRow(account, idp_display_data, /*should_hover=*/false));
  AddChildView(std::move(row));
  SizeToContents();
  PreferredSizeChanged();

  SendAccessibilityEvent(GetWidget(), title);

  has_sheet_ = true;
}

void AccountSelectionBubbleView::ShowSingleAccountConfirmDialog(
    const std::u16string& top_frame_for_display,
    const absl::optional<std::u16string>& iframe_for_display,
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    bool show_back_button) {
  std::u16string title =
      GetTitle(top_frame_for_display, iframe_for_display,
               idp_display_data.idp_etld_plus_one, rp_context_);
  UpdateHeader(idp_display_data.idp_metadata, title, subtitle_,
               show_back_button);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateSingleAccountChooser(idp_display_data, account));
  SizeToContents();
  PreferredSizeChanged();

  if (has_sheet_) {
    // Focusing `continue_button_` without screen reader on makes the UI look
    // awkward, so we only want to do so when screen reader is enabled.
    if (accessibility_state_utils::IsScreenReaderEnabled() && continue_button_)
      continue_button_->RequestFocus();
    SendAccessibilityEvent(GetWidget(), std::u16string());
  }

  has_sheet_ = true;
}

void AccountSelectionBubbleView::ShowFailureDialog(
    const std::u16string& top_frame_for_display,
    const absl::optional<std::u16string>& iframe_for_display,
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  int subtitleLeftPadding = 2 * kLeftRightPadding;
  if (header_icon_view_) {
    ConfigureIdpBrandImageView(header_icon_view_, idp_metadata);
    subtitleLeftPadding += kDesiredIdpIconSize;
  }

  std::u16string frame_in_title =
      iframe_for_display.value_or(top_frame_for_display);
  const std::u16string title =
      l10n_util::GetStringFUTF16(IDS_IDP_SIGNIN_STATUS_FAILURE_DIALOG_TITLE,
                                 frame_in_title, idp_for_display);
  title_label_->SetText(title);

  if (subtitle_label_) {
    subtitle_label_->SetText(subtitle_);
    subtitle_label_->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(-kTopBottomPadding, subtitleLeftPadding,
                          kTopBottomPadding, kLeftRightPadding)));
  }

  RemoveNonHeaderChildViews();
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kTopBottomPadding, kLeftRightPadding)));
  auto button = std::make_unique<ContinueButton>(
      base::BindRepeating(&Observer::OnSigninToIdP,
                          base::Unretained(observer_)),
      l10n_util::GetStringUTF16(IDS_IDP_SIGNIN_STATUS_FAILURE_DIALOG_CONTINUE),
      this, idp_metadata);
  signin_to_idp_button_ = row->AddChildView(std::move(button));
  AddChildView(std::move(row));

  SizeToContents();
  PreferredSizeChanged();

  has_sheet_ = true;
}

void AccountSelectionBubbleView::AddIdpImage(const GURL& image_url,
                                             gfx::ImageSkia image) {
  idp_images_[image_url] = image;
}

std::string AccountSelectionBubbleView::GetDialogTitle() const {
  // We cannot just return title_ because it is not always set
  // (e.g. by ShowFailureDialog).
  return base::UTF16ToUTF8(title_label_->GetText());
}

absl::optional<std::string> AccountSelectionBubbleView::GetDialogSubtitle()
    const {
  if (!subtitle_label_) {
    return absl::nullopt;
  }

  return base::UTF16ToUTF8(subtitle_label_->GetText());
}

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
  // In the RTL case, the bubble is aligned towards the left side of the screen
  // and hence the x-axis offset needs to be in the opposite direction.
  return views::BubbleDialogDelegateView::GetBubbleBounds() +
         gfx::Vector2d(base::i18n::IsRTL() ? kRightMargin : -kRightMargin,
                       GetWidget()->client_view()->GetPreferredSize().height() +
                           kTopMargin);
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateHeaderView(
    bool has_idp_icon) {
  auto header = std::make_unique<views::View>();
  // Do not use a top margin as it has already been set in the bubble.
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets::TLBR(
          0, kLeftRightPadding, kVerticalSpacing, kLeftRightPadding));

  // Add the space for the icon.
  if (has_idp_icon) {
    auto image_view = std::make_unique<IdpImageView>(this);
    image_view->SetImageSize(
        gfx::Size(kDesiredIdpIconSize, kDesiredIdpIconSize));
    image_view->SetProperty(views::kMarginsKey,
                            gfx::Insets().set_right(kLeftRightPadding));
    header_icon_view_ = header->AddChildView(std::move(image_view));
  }

  back_button_ =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&Observer::OnBackButtonClicked,
                              base::Unretained(observer_)),
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
      title_, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  SetTitleHeaderProperties(title_label_);

  // Add the close button.
  std::unique_ptr<views::Button> close_button =
      views::BubbleFrameView::CreateCloseButton(base::BindRepeating(
          &Observer::OnCloseButtonClicked, base::Unretained(observer_)));
  close_button->SetVisible(true);
  header->AddChildView(std::move(close_button));

  if (subtitle_.empty()) {
    return header;
  }

  // Add the subtitle.
  auto header_with_subtitle = std::make_unique<views::View>();
  header_with_subtitle->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  header_with_subtitle->AddChildView(std::move(header));
  subtitle_label_ =
      header_with_subtitle->AddChildView(std::make_unique<views::Label>(
          subtitle_, views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
  SetTitleHeaderProperties(subtitle_label_);
  int leftPadding = 2 * kLeftRightPadding;
  if (has_idp_icon) {
    leftPadding += kDesiredIdpIconSize;
  }
  subtitle_label_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      -kTopBottomPadding, leftPadding, kTopBottomPadding, kLeftRightPadding)));

  return header_with_subtitle;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateSingleAccountChooser(
    const IdentityProviderDisplayData& idp_display_data,
    const content::IdentityRequestAccount& account) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kLeftRightPadding), kVerticalSpacing));
  row->AddChildView(
      CreateAccountRow(account, idp_display_data, /*should_hover=*/false));

  // Prefer using the given name if it is provided, otherwise fallback to name.
  const std::string display_name =
      account.given_name.empty() ? account.name : account.given_name;
  const content::IdentityProviderMetadata& idp_metadata =
      idp_display_data.idp_metadata;
  // We can pass crefs to OnAccountSelected because the `observer_` owns the
  // data.
  auto button = std::make_unique<ContinueButton>(
      base::BindRepeating(&Observer::OnAccountSelected,
                          base::Unretained(observer_), std::cref(account),
                          std::cref(idp_display_data)),
      l10n_util::GetStringFUTF16(IDS_ACCOUNT_SELECTION_CONTINUE,
                                 base::UTF8ToUTF16(display_name)),
      this, idp_metadata);
  continue_button_ = row->AddChildView(std::move(button));

  if (show_auto_reauthn_checkbox_) {
    auto_reauthn_checkbox_ =
        row->AddChildView(std::make_unique<views::Checkbox>(
            l10n_util::GetStringUTF16(IDS_AUTO_REAUTHN_OPTOUT_CHECKBOX)));
    auto_reauthn_checkbox_->SetChecked(true);
  }

  // Do not add disclosure text if this is a sign in.
  if (account.login_state == Account::LoginState::kSignIn)
    return row;

  // Add disclosure text. It requires a StyledLabel so that we can add the links
  // to the privacy policy and terms of service URLs.
  views::StyledLabel* const disclosure_label =
      row->AddChildView(std::make_unique<views::StyledLabel>());
  disclosure_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);

  // Set custom top margin for `disclosure_label` in order to take
  // (line_height - font_height) into account.
  disclosure_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(5, 0, 0, 0)));
  disclosure_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);

  const content::ClientMetadata& client_metadata =
      idp_display_data.client_metadata;
  int disclosure_resource_id = SelectDisclosureTextResourceId(
      client_metadata.privacy_policy_url, client_metadata.terms_of_service_url);

  // The order that the links are added to `link_data` should match the order of
  // the links in `disclosure_resource_id`.
  std::vector<std::pair<Observer::LinkType, GURL>> link_data;
  if (!client_metadata.privacy_policy_url.is_empty()) {
    link_data.emplace_back(Observer::LinkType::PRIVACY_POLICY,
                           client_metadata.privacy_policy_url);
  }
  if (!client_metadata.terms_of_service_url.is_empty()) {
    link_data.emplace_back(Observer::LinkType::TERMS_OF_SERVICE,
                           client_metadata.terms_of_service_url);
  }

  // Each link has both <ph name="BEGIN_LINK"> and <ph name="END_LINK">.
  std::vector<std::u16string> replacements = {
      idp_display_data.idp_etld_plus_one};
  replacements.insert(replacements.end(), link_data.size() * 2,
                      std::u16string());

  std::vector<size_t> offsets;
  const std::u16string disclosure_text = l10n_util::GetStringFUTF16(
      disclosure_resource_id, replacements, &offsets);
  disclosure_label->SetText(disclosure_text);

  size_t offset_index = 1u;
  for (const std::pair<Observer::LinkType, GURL>& link_data_item : link_data) {
    disclosure_label->AddStyleRange(
        gfx::Range(offsets[offset_index], offsets[offset_index + 1]),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &Observer::OnLinkClicked, base::Unretained(observer_),
            link_data_item.first, link_data_item.second)));
    offset_index += 2;
  }

  return row;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateMultipleAccountChooser(
    const std::vector<IdentityProviderDisplayData>& idp_display_data_list) {
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  views::View* const row =
      scroll_view->SetContents(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  bool is_multi_idp = idp_display_data_list.size() > 1u;
  size_t num_rows = 0;
  for (const auto& idp_display_data : idp_display_data_list) {
    if (is_multi_idp) {
      row->AddChildView(CreateIdpHeaderRowForMultiIdp(
          idp_display_data.idp_etld_plus_one, idp_display_data.idp_metadata));
      ++num_rows;
    }
    for (const auto& account : idp_display_data.accounts) {
      row->AddChildView(
          CreateAccountRow(account, idp_display_data, /*should_hover=*/true));
    }
    num_rows += idp_display_data.accounts.size();
  }
  // The maximum height that the multi-account-picker can have. This value was
  // chosen so that if there are more than two accounts, the picker will show up
  // as a scrollbar showing 2 accounts plus half of the third one. Note that
  // this is an estimate if there are multiple IDPs, as IDP rows are not the
  // same height. That said, calling GetPreferredSize() is expensive so we are
  // ok with this estimate. And in this case, we prefer to use 3.5 as there will
  // be at least one IDP row at the beginning.
  float num_visible_rows = is_multi_idp ? 3.5f : 2.5f;
  const int per_account_size = row->GetPreferredSize().height() / num_rows;
  scroll_view->ClipHeightTo(
      0, static_cast<int>(per_account_size * num_visible_rows));
  return scroll_view;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateIdpHeaderRowForMultiIdp(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  auto header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(
          gfx::Insets::TLBR(0, kLeftRightPadding, 0, kLeftRightPadding));

  auto image_view = std::make_unique<IdpImageView>(this);
  image_view->SetImageSize(gfx::Size(kDesiredIdpIconSize, kDesiredIdpIconSize));
  image_view->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_right(kLeftRightPadding));
  IdpImageView* idp_icon_view = header->AddChildView(std::move(image_view));
  ConfigureIdpBrandImageView(idp_icon_view, idp_metadata);

  header->AddChildView(std::make_unique<views::Label>(
      idp_for_display, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY));
  return header;
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateAccountRow(
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    bool should_hover) {
  auto image_view = std::make_unique<AccountImageView>();
  image_view->SetImageSize({kDesiredAvatarSize, kDesiredAvatarSize});
  image_view->FetchImage(account, *image_fetcher_);
  if (should_hover) {
    // We can pass crefs to OnAccountSelected because the `observer_` owns the
    // data.
    auto row = std::make_unique<HoverButton>(
        base::BindRepeating(&Observer::OnAccountSelected,
                            base::Unretained(observer_), std::cref(account),
                            std::cref(idp_display_data)),
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
  views::View* const text_column =
      row->AddChildView(std::make_unique<views::View>());
  text_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Add account name.
  views::Label* const account_name =
      text_column->AddChildView(std::make_unique<views::Label>(
          base::UTF8ToUTF16(account.name),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  account_name->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Add account email.
  views::Label* const account_email = text_column->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(account.email),
                                     views::style::CONTEXT_DIALOG_BODY_TEXT,
                                     views::style::STYLE_SECONDARY));
  account_email->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  return row;
}

void AccountSelectionBubbleView::UpdateHeader(
    const content::IdentityProviderMetadata& idp_metadata,
    const std::u16string subpage_title,
    const std::u16string subpage_subtitle,
    bool show_back_button) {
  back_button_->SetVisible(show_back_button);
  if (header_icon_view_) {
    if (show_back_button)
      header_icon_view_->SetVisible(false);
    else
      ConfigureIdpBrandImageView(header_icon_view_, idp_metadata);
  }
  title_label_->SetText(subpage_title);

  if (subtitle_label_) {
    if (subpage_subtitle.empty()) {
      delete subtitle_label_;
      subtitle_label_ = nullptr;
      return;
    }
    subtitle_label_->SetText(subpage_subtitle);
  }
}

void AccountSelectionBubbleView::ConfigureIdpBrandImageView(
    IdpImageView* image_view,
    const content::IdentityProviderMetadata& idp_metadata) {
  // Show placeholder brand icon prior to brand icon being fetched so that
  // header text wrapping does not change when brand icon is fetched.
  bool has_idp_icon = idp_metadata.brand_icon_url.is_valid();
  image_view->SetVisible(has_idp_icon);
  if (!has_idp_icon)
    return;

  auto it = idp_images_.find(idp_metadata.brand_icon_url);
  if (it != idp_images_.end()) {
    image_view->SetImage(it->second);
    return;
  }

  image_view->FetchImage(idp_metadata.brand_icon_url, *image_fetcher_);
}

void AccountSelectionBubbleView::RemoveNonHeaderChildViews() {
  const std::vector<views::View*> child_views = children();
  for (views::View* child_view : child_views) {
    if (child_view != header_view_) {
      RemoveChildView(child_view);
      delete child_view;
    }
  }

  continue_button_ = nullptr;
}

BEGIN_METADATA(AccountSelectionBubbleView, views::BubbleDialogDelegateView)
END_METADATA
