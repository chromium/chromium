// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_view_base.h"

#include "base/functional/callback_forward.h"
#include "base/i18n/message_formatter.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace {

// safe_zone_diameter/icon_size as defined in
// https://www.w3.org/TR/appmanifest/#icon-masks
constexpr float kMaskableWebIconSafeZoneRatio = 0.8f;

// The border radius of the background circle containing the IDP icon in an
// account button.
constexpr int kIdpBorderRadius = 10;

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

std::u16string GetPermissionFieldsString(
    const std::vector<content::IdentityRequestDialogDisclosureField>& fields) {
  std::vector<std::string> strings;
  for (auto field : fields) {
    switch (field) {
      case content::IdentityRequestDialogDisclosureField::kName:
        strings.push_back(
            l10n_util::GetStringUTF8(IDS_ACCOUNT_SELECTION_DATA_SHARING_NAME));
        break;
      case content::IdentityRequestDialogDisclosureField::kEmail:
        strings.push_back(
            l10n_util::GetStringUTF8(IDS_ACCOUNT_SELECTION_DATA_SHARING_EMAIL));
        break;
      case content::IdentityRequestDialogDisclosureField::kPicture:
        strings.push_back(l10n_util::GetStringUTF8(
            IDS_ACCOUNT_SELECTION_DATA_SHARING_PICTURE));
        break;
    }
  }
  // Make sure we have at least 3 strings in the vector for the function call.
  int num_strings = strings.size();
  if (strings.size() < 3) {
    strings.resize(3);
  }
  return base::i18n::MessageFormatter::FormatWithNamedArgs(
      l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_DATA_SHARING_STRING),
      "count", num_strings, "field_1", strings[0], "field_2", strings[1],
      "field_3", strings[2]);
}

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
          internal {
            contacts {
                email: "web-identity-eng@google.com"
            }
          }
          user_data {
            type: USER_CONTENT
          }
          last_reviewed: "2024-01-25"
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
                               std::optional<int> pre_resize_avatar_crop_size,
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
      if (height_ratio >= 1.0f) {
        scaled_height = floor(canvas_edge_size * height_ratio);
      } else {
        scaled_width = floor(canvas_edge_size / height_ratio);
      }
    }
    avatar_ = gfx::ImageSkiaOperations::CreateResizedImage(
        avatar, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(scaled_width, scaled_height));
  }

  CircleCroppedImageSkiaSource(const CircleCroppedImageSkiaSource&) = delete;
  CircleCroppedImageSkiaSource& operator=(const CircleCroppedImageSkiaSource&) =
      delete;
  ~CircleCroppedImageSkiaSource() override = default;

  // CanvasImageSource:
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

gfx::ImageSkia CreateCircleCroppedImage(const gfx::ImageSkia& original_image,
                                        int image_size) {
  return gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
      original_image, original_image.width() * kMaskableWebIconSafeZoneRatio,
      image_size);
}

class AccountImageView : public views::ImageView {
  METADATA_HEADER(AccountImageView, views::ImageView)

 public:
  AccountImageView() = default;

  AccountImageView(const AccountImageView&) = delete;
  AccountImageView& operator=(const AccountImageView&) = delete;
  ~AccountImageView() override = default;

  // Check image and set it on AccountImageView.
  void SetAccountImage(const content::IdentityRequestAccount& account,
                       image_fetcher::ImageFetcher& image_fetcher,
                       int image_size) {
    gfx::ImageSkia avatar;
    if (account.decoded_picture.IsEmpty()) {
      std::u16string letter = base::UTF8ToUTF16(account.name);
      if (letter.length() > 0) {
        letter = base::i18n::ToUpper(letter.substr(0, 1));
      }
      avatar = gfx::CanvasImageSource::MakeImageSkia<
          LetterCircleCroppedImageSkiaSource>(letter, image_size);
    } else {
      avatar =
          gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
              account.decoded_picture.AsImageSkia(), std::nullopt, image_size);
    }
    SetImage(ui::ImageModel::FromImageSkia(avatar));
  }

 private:
  base::WeakPtrFactory<AccountImageView> weak_ptr_factory_{this};
};

BEGIN_METADATA(AccountImageView)
END_METADATA

class AccountHoverButton : public HoverButton {
 public:
  AccountHoverButton(PressedCallback callback,
                     std::unique_ptr<views::View> icon_view,
                     const std::u16string& title,
                     const std::u16string& subtitle,
                     std::unique_ptr<views::View> secondary_view,
                     bool add_vertical_label_spacing,
                     const std::u16string& footer,
                     BrandIconImageView* brand_icon_image_view,
                     int button_position)
      : HoverButton(base::BindRepeating(&AccountHoverButton::OnPressed,
                                        base::Unretained(this)),
                    std::move(icon_view),
                    title,
                    subtitle,
                    std::move(secondary_view),
                    add_vertical_label_spacing,
                    footer),
        callback_(std::move(callback)),
        brand_icon_image_view_(brand_icon_image_view),
        button_position_(button_position) {}

  AccountHoverButton(const AccountHoverButton&) = delete;
  AccountHoverButton& operator=(const AccountHoverButton&) = delete;
  ~AccountHoverButton() override = default;

  void StateChanged(ButtonState old_state) override {
    // If there is an IDP icon within the account button, the IDP icon was
    // created using a background circle with the color of the background. When
    // the button state changes, the color of the background may change, so we
    // recreate the background circle.
    HoverButton::StateChanged(old_state);
    if (brand_icon_image_view_) {
      ui::ColorProvider* provider =
          brand_icon_image_view_->parent()->GetColorProvider();
      if (provider) {
        ui::ColorId color_id;
        switch (GetState()) {
          case ButtonState::STATE_NORMAL: {
            color_id = ui::kColorDialogBackground;
            break;
          }
          case ButtonState::STATE_HOVERED:
          case ButtonState::STATE_PRESSED: {
            color_id = ui::kColorMenuButtonBackgroundSelected;
            break;
          }
          case ButtonState::STATE_DISABLED:
          default: {
            return;
          }
        }
        brand_icon_image_view_->OnBackgroundColorUpdated(
            provider->GetColor(color_id));
      }
    }
  }

  void OnThemeChanged() override {
    HoverButton::OnThemeChanged();
    if (brand_icon_image_view_) {
      ui::ColorProvider* provider =
          brand_icon_image_view_->parent()->GetColorProvider();
      if (provider) {
        brand_icon_image_view_->OnBackgroundColorUpdated(
            provider->GetColor(ui::kColorDialogBackground));
      }
    }
  }

  void OnPressed(const ui::Event& event) {
    // Log the metric before invoking the callback since the callback may
    // destroy this object.
    base::UmaHistogramCustomCounts("Blink.FedCm.AccountChosenPosition.Desktop",
                                   button_position_,
                                   /*min=*/0,
                                   /*exclusive_max=*/10, /*buckets=*/11);
    if (callback_) {
      callback_.Run(event);
    }
  }

 private:
  PressedCallback callback_;
  // Owned by its views::BoxLayoutView container.
  raw_ptr<BrandIconImageView> brand_icon_image_view_;
  // The order of this account button relative to other account buttons in
  // the dialog (e.g. 0 is the topmost account, 1 the one below it, etc.). Used
  // to record a metric when the button is clicked.
  int button_position_;
};

}  // namespace

BrandIconImageView::BrandIconImageView(
    base::OnceCallback<void(const GURL&, const gfx::ImageSkia&)> add_image,
    int image_size,
    bool should_circle_crop,
    std::optional<SkColor> background_color,
    base::RepeatingClosure on_image_set)
    : add_image_(std::move(add_image)),
      image_size_(image_size),
      should_circle_crop_(should_circle_crop),
      background_color_(background_color),
      on_image_set_(std::move(on_image_set)) {}

BrandIconImageView::~BrandIconImageView() = default;

void BrandIconImageView::FetchImage(
    const GURL& icon_url,
    image_fetcher::ImageFetcher& image_fetcher) {
  image_fetcher::ImageFetcherParams params(kTrafficAnnotation,
                                           kImageFetcherUmaClient);
  image_fetcher.FetchImage(
      icon_url,
      base::BindOnce(&BrandIconImageView::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), icon_url),
      std::move(params));
}

void BrandIconImageView::CropAndSetImage(const gfx::ImageSkia& original_image) {
  cropped_idp_image_ =
      should_circle_crop_
          ? CreateCircleCroppedImage(original_image, image_size_)
          : gfx::ImageSkiaOperations::CreateResizedImage(
                original_image, skia::ImageOperations::RESIZE_BEST,
                gfx::Size(image_size_, image_size_));
  SetImage(ui::ImageModel::FromImageSkia(
      background_color_
          ? gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
                kIdpBorderRadius, *background_color_, cropped_idp_image_)
          : cropped_idp_image_));

  if (!on_image_set_) {
    return;
  }
  std::move(on_image_set_).Run();
}

void BrandIconImageView::OnImageFetched(
    const GURL& image_url,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (image.Width() != image.Height() ||
      image.Width() < (image_size_ / kMaskableWebIconSafeZoneRatio)) {
    return;
  }
  gfx::ImageSkia skia_image = image.AsImageSkia();
  CropAndSetImage(skia_image);

  // TODO(crbug.com/327509202): This stops the crashes but should fix to prevent
  // this from crashing in the first place.
  if (!add_image_) {
    return;
  }
  std::move(add_image_).Run(image_url, skia_image);
}

void BrandIconImageView::OnBackgroundColorUpdated(
    const SkColor& background_color) {
  if (!background_color_) {
    return;
  }
  background_color_ = background_color;
  SetImage(ui::ImageModel::FromImageSkia(
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          kIdpBorderRadius, *background_color_, cropped_idp_image_)));
}

BEGIN_METADATA(BrandIconImageView)
END_METADATA

AccountSelectionViewBase::AccountSelectionViewBase(
    content::WebContents* web_contents,
    AccountSelectionViewBase::Observer* observer,
    views::WidgetObserver* widget_observer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::u16string rp_for_display)
    : web_contents_(web_contents->GetWeakPtr()),
      widget_observer_(widget_observer),
      observer_(observer),
      rp_for_display_(rp_for_display) {
  image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(), std::move(url_loader_factory));
}

AccountSelectionViewBase::AccountSelectionViewBase() = default;
AccountSelectionViewBase::~AccountSelectionViewBase() {}

void AccountSelectionViewBase::OnOcclusionStateChanged(bool occluded) {
  if (dialog_widget_) {
    dialog_widget_->GetContentsView()->SetEnabled(!occluded);
  }
  // SetEnabled does not always seem sufficient for unknown reasons, so we
  // also set this boolean to ignore input. But we still call SetEnabled
  // to visually indicate that input is disabled where possible.
  is_occluded_ = occluded;
}

void AccountSelectionViewBase::SetLabelProperties(views::Label* label) {
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
}

std::unique_ptr<views::View> AccountSelectionViewBase::CreateAccountRow(
    const content::IdentityRequestAccount& account,
    std::optional<int> clickable_position,
    bool should_include_idp,
    bool is_modal_dialog,
    int additional_vertical_padding,
    std::optional<std::u16string> last_used_string) {
  int avatar_size = is_modal_dialog ? kModalAvatarSize : kDesiredAvatarSize;
  views::style::TextStyle account_name_style =
      is_modal_dialog ? views::style::STYLE_BODY_3_MEDIUM
                      : views::style::STYLE_PRIMARY;
  views::style::TextStyle account_email_style =
      is_modal_dialog ? views::style::STYLE_BODY_5
                      : views::style::STYLE_SECONDARY;

  std::unique_ptr<views::View> avatar_view;
  auto account_image_view = std::make_unique<AccountImageView>();
  account_image_view->SetImageSize({avatar_size, avatar_size});
  CHECK(clickable_position || !should_include_idp);
  const content::IdentityProviderData& idp_data = *account.identity_provider;
  if (clickable_position) {
    BrandIconImageView* brand_icon_image_view_ptr = nullptr;
    if (should_include_idp) {
      account_image_view->SetAccountImage(account, *image_fetcher_,
                                          avatar_size);
      // Introduce a border so that the IDP image is a bit past the account
      // image.
      account_image_view->SetBorder(views::CreateEmptyBorder(
          gfx::Insets::TLBR(/*top=*/0, /*left=*/0, /*bottom=*/kIdpBadgeOffset,
                            /*right=*/kIdpBadgeOffset)));
      // Put `account_image_view` into a FillLayout `background_container`.
      std::unique_ptr<views::View> background_container =
          std::make_unique<views::View>();
      background_container->SetUseDefaultFillLayout(true);
      background_container->AddChildView(std::move(account_image_view));

      // Put brand icon image view into a BoxLayout container.
      std::unique_ptr<views::BoxLayoutView> icon_container =
          std::make_unique<views::BoxLayoutView>();
      icon_container->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
      icon_container->SetCrossAxisAlignment(views::LayoutAlignment::kEnd);

      // `web_contents_` may be nullptr in tests.
      SkColor background_color =
          web_contents_ ? web_contents_->GetColorProvider().GetColor(
                              ui::kColorDialogBackground)
                        : SK_ColorWHITE;
      std::unique_ptr<BrandIconImageView> brand_icon_image_view =
          std::make_unique<BrandIconImageView>(
              base::BindOnce(&AccountSelectionViewBase::AddIdpImage,
                             weak_ptr_factory_.GetWeakPtr()),
              kLargeAvatarBadgeSize, /*should_circle_crop=*/true,
              background_color);
      brand_icon_image_view_ptr = brand_icon_image_view.get();
      ConfigureBrandImageView(brand_icon_image_view_ptr,
                              idp_data.idp_metadata.brand_icon_url);

      icon_container->AddChildView(std::move(brand_icon_image_view));

      // Put BoxLayout container into FillLayout container to stack the views.
      // This stacks the IDP icon on top of the background image.
      background_container->AddChildView(std::move(icon_container));

      avatar_view = std::move(background_container);
    } else {
      account_image_view->SetAccountImage(account, *image_fetcher_,
                                          avatar_size);
      avatar_view = std::move(account_image_view);
    }
    std::unique_ptr<views::ImageView> arrow_icon_view = nullptr;
    if (is_modal_dialog) {
      constexpr int kArrowIconRightPadding = 8;
      arrow_icon_view = std::make_unique<views::ImageView>();
      arrow_icon_view->SetBorder(views::CreateEmptyBorder(
          gfx::Insets::TLBR(/*top=*/0, /*left=*/0, /*bottom=*/0,
                            /*right=*/kArrowIconRightPadding)));
      arrow_icon_view->SetImage(ui::ImageModel::FromVectorIcon(
          vector_icons::kSubmenuArrowIcon, ui::kColorIcon, kArrowIconSize));
    }

    std::u16string footer = u"";
    if (should_include_idp) {
      if (last_used_string) {
        footer = l10n_util::GetStringFUTF16(
            IDS_MULTI_IDP_ACCOUNT_ORIGIN_AND_LAST_USED,
            base::UTF8ToUTF16(idp_data.idp_for_display), *last_used_string);
      } else {
        footer = base::UTF8ToUTF16(idp_data.idp_for_display);
      }
    }
    // We can pass crefs to OnAccountSelected because the `observer_` owns the
    // data.
    auto row = std::make_unique<AccountHoverButton>(
        base::BindRepeating(
            &AccountSelectionViewBase::Observer::OnAccountSelected,
            base::Unretained(observer_), std::cref(account),
            std::cref(idp_data)),
        std::move(avatar_view),
        /*title=*/base::UTF8ToUTF16(account.name),
        /*subtitle=*/base::UTF8ToUTF16(account.email),
        /*secondary_view=*/std::move(arrow_icon_view),
        /*add_vertical_label_spacing=*/true, footer, brand_icon_image_view_ptr,
        *clickable_position);
    row->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
        /*vertical=*/additional_vertical_padding,
        /*horizontal=*/is_modal_dialog ? kModalHorizontalSpacing
                                       : kLeftRightPadding)));
    row->SetTitleTextStyle(account_name_style, ui::kColorDialogBackground,
                           /*color_id=*/std::nullopt);
    row->SetSubtitleTextStyle(views::style::CONTEXT_LABEL, account_email_style);
    if (should_include_idp) {
      row->SetFooterTextStyle(views::style::CONTEXT_LABEL, account_email_style);
    }
    return row;
  }
  account_image_view->SetAccountImage(account, *image_fetcher_, avatar_size);
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(
          /*vertical=*/kVerticalSpacing + additional_vertical_padding,
          /*horizontal=*/is_modal_dialog ? kModalHorizontalSpacing : 0),
      kLeftRightPadding));
  row->AddChildView(std::move(account_image_view));
  views::View* const text_column =
      row->AddChildView(std::make_unique<views::View>());
  text_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Add account name.
  views::StyledLabel* const account_name =
      text_column->AddChildView(std::make_unique<views::StyledLabel>());
  account_name->SetDefaultTextStyle(account_name_style);
  account_name->SetText(base::UTF8ToUTF16(account.name));
  account_name->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Add account email.
  views::Label* const account_email =
      text_column->AddChildView(std::make_unique<views::Label>(
          base::UTF8ToUTF16(account.email),
          views::style::CONTEXT_DIALOG_BODY_TEXT, account_email_style));
  account_email->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  return row;
}

void AccountSelectionViewBase::AddIdpImage(const GURL& image_url,
                                           const gfx::ImageSkia& image) {
  brand_icon_images_[image_url] = image;
}

void AccountSelectionViewBase::ConfigureBrandImageView(
    BrandIconImageView* image_view,
    const GURL& brand_icon_url) {
  bool is_valid_icon_url = brand_icon_url.is_valid();
  if (!is_valid_icon_url) {
    return;
  }

  auto it = brand_icon_images_.find(brand_icon_url);
  if (it != brand_icon_images_.end()) {
    image_view->CropAndSetImage(it->second);
    return;
  }

  image_view->FetchImage(brand_icon_url, *image_fetcher_);
}

std::unique_ptr<views::StyledLabel>
AccountSelectionViewBase::CreateDisclosureLabel(
    const content::IdentityProviderData& idp_data) {
  // It requires a StyledLabel so that we can add the links
  // to the privacy policy and terms of service URLs.
  std::unique_ptr<views::StyledLabel> disclosure_label =
      std::make_unique<views::StyledLabel>();
  disclosure_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);

  // Set custom top margin for `disclosure_label` in order to take
  // (line_height - font_height) into account.
  disclosure_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(5, 0, 0, 0)));
  disclosure_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);

  const content::ClientMetadata& client_metadata = idp_data.client_metadata;
  int disclosure_resource_id = SelectDisclosureTextResourceId(
      client_metadata.privacy_policy_url, client_metadata.terms_of_service_url);

  // The order that the links are added to `link_data` should match the order of
  // the links in `disclosure_resource_id`.
  std::vector<std::pair<LinkType, GURL>> link_data;
  if (!client_metadata.privacy_policy_url.is_empty()) {
    link_data.emplace_back(LinkType::PRIVACY_POLICY,
                           client_metadata.privacy_policy_url);
  }
  if (!client_metadata.terms_of_service_url.is_empty()) {
    link_data.emplace_back(LinkType::TERMS_OF_SERVICE,
                           client_metadata.terms_of_service_url);
  }

  // Each link has both <ph name="BEGIN_LINK"> and <ph name="END_LINK">.
  std::vector<std::u16string> replacements = {
      base::UTF8ToUTF16(idp_data.idp_for_display),
      GetPermissionFieldsString(idp_data.disclosure_fields)};
  replacements.insert(replacements.end(), link_data.size() * 2,
                      std::u16string());

  std::vector<size_t> offsets;
  const std::u16string disclosure_text = l10n_util::GetStringFUTF16(
      disclosure_resource_id, replacements, &offsets);
  disclosure_label->SetText(disclosure_text);

  size_t offset_index = 2u;
  for (const std::pair<LinkType, GURL>& link_data_item : link_data) {
    disclosure_label->AddStyleRange(
        gfx::Range(offsets[offset_index], offsets[offset_index + 1]),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &AccountSelectionViewBase::Observer::OnLinkClicked,
            base::Unretained(observer_), link_data_item.first,
            link_data_item.second)));
    offset_index += 2;
  }

  return disclosure_label;
}

base::WeakPtr<views::Widget> AccountSelectionViewBase::GetDialogWidget() {
  return dialog_widget_;
}

// static
net::NetworkTrafficAnnotationTag
AccountSelectionViewBase::GetTrafficAnnotation() {
  return kTrafficAnnotation;
}

bool AccountSelectionViewBase::CanFitInWebContents() {
  CHECK(web_contents_ && dialog_widget_);

  gfx::Size web_contents_size = web_contents_->GetSize();
  gfx::Size preferred_bubble_size =
      dialog_widget_->GetContentsView()->GetPreferredSize();

  // TODO(crbug.com/340368623): Figure out what to do when button flow modal
  // cannot fit in web contents. The offsets kRightMargin and kTopMargin pertain
  // to the bubble widget.
  return preferred_bubble_size.width() <
             (web_contents_size.width() - kRightMargin) &&
         preferred_bubble_size.height() <
             (web_contents_size.height() - kTopMargin);
}
