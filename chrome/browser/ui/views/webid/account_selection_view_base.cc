// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_view_base.h"

#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget_observer.h"

// safe_zone_diameter/icon_size as defined in
// https://www.w3.org/TR/appmanifest/#icon-masks
constexpr float kMaskableWebIconSafeZoneRatio = 0.8f;

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

void LetterCircleCroppedImageSkiaSource::Draw(gfx::Canvas* canvas) {
  monogram::DrawMonogramInCanvas(canvas, size().width(), size().width(),
                                 letter_, SK_ColorWHITE, SK_ColorGRAY);
}

void CircleCroppedImageSkiaSource::Draw(gfx::Canvas* canvas) {
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

BrandIconImageView::BrandIconImageView(
    base::OnceCallback<void(const GURL&, const gfx::ImageSkia&)> add_idp_image,
    int image_size)
    : add_idp_image_(std::move(add_idp_image)), image_size_(image_size) {}

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

void BrandIconImageView::OnImageFetched(
    const GURL& image_url,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (image.Width() != image.Height() ||
      image.Width() < AccountSelectionView::GetBrandIconMinimumSize()) {
    return;
  }
  gfx::ImageSkia idp_image =
      gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
          image.AsImageSkia(), image.Width() * kMaskableWebIconSafeZoneRatio,
          image_size_);
  SetImage(ui::ImageModel::FromImageSkia(idp_image));
  CHECK(add_idp_image_);
  std::move(add_idp_image_).Run(image_url, idp_image);
}

BEGIN_METADATA(BrandIconImageView)
END_METADATA

class AccountImageView : public views::ImageView {
  METADATA_HEADER(AccountImageView, views::ImageView)

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
      if (letter.length() > 0) {
        letter = base::i18n::ToUpper(account_name.substr(0, 1));
      }
      avatar = gfx::CanvasImageSource::MakeImageSkia<
          LetterCircleCroppedImageSkiaSource>(letter, kDesiredAvatarSize);
    } else {
      avatar =
          gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
              image.AsImageSkia(), std::nullopt, kDesiredAvatarSize);
    }
    SetImage(ui::ImageModel::FromImageSkia(avatar));
  }

  base::WeakPtrFactory<AccountImageView> weak_ptr_factory_{this};
};

BEGIN_METADATA(AccountImageView)
END_METADATA

AccountSelectionViewBase::AccountSelectionViewBase(
    content::WebContents* web_contents,
    AccountSelectionViewBase::Observer* observer,
    views::WidgetObserver* widget_observer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : web_contents_(web_contents),
      widget_observer_(widget_observer),
      observer_(observer) {
  image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(), std::move(url_loader_factory));
}

AccountSelectionViewBase::AccountSelectionViewBase() = default;
AccountSelectionViewBase::~AccountSelectionViewBase() {}

int AccountSelectionViewBase::SelectSingleIdpTitleResourceId(
    blink::mojom::RpContext rp_context) {
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

// Returns the title to be shown in the dialog. This does not include the
// subtitle. For screen reader purposes, GetAccessibleTitle() is used instead.
std::u16string AccountSelectionViewBase::GetTitle(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const std::optional<std::u16string>& idp_title,
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

std::u16string AccountSelectionViewBase::GetSubtitle(
    const std::u16string& top_frame_for_display) {
  return l10n_util::GetStringFUTF16(IDS_ACCOUNT_SELECTION_SHEET_SUBTITLE,
                                    top_frame_for_display);
}

// Returns the title combined with the subtitle for screen reader purposes.
std::u16string AccountSelectionViewBase::GetAccessibleTitle(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context) {
  std::u16string title = GetTitle(top_frame_for_display, iframe_for_display,
                                  idp_title, rp_context);
  return iframe_for_display.has_value()
             ? title + u" " + GetSubtitle(top_frame_for_display)
             : title;
}

void AccountSelectionViewBase::SetLabelProperties(views::Label* label) {
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true));
}

std::unique_ptr<views::View> AccountSelectionViewBase::CreateAccountRow(
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
        base::BindRepeating(
            &AccountSelectionViewBase::Observer::OnAccountSelected,
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

void AccountSelectionViewBase::AddIdpImage(const GURL& image_url,
                                           const gfx::ImageSkia& image) {
  idp_images_[image_url] = image;
}

void AccountSelectionViewBase::ConfigureIdpBrandImageView(
    BrandIconImageView* image_view,
    const content::IdentityProviderMetadata& idp_metadata) {
  // Show placeholder brand icon prior to brand icon being fetched so that
  // header text wrapping does not change when brand icon is fetched.
  bool has_idp_icon = idp_metadata.brand_icon_url.is_valid();
  image_view->SetVisible(has_idp_icon);
  if (!has_idp_icon) {
    return;
  }

  auto it = idp_images_.find(idp_metadata.brand_icon_url);
  if (it != idp_images_.end()) {
    image_view->SetImage(ui::ImageModel::FromImageSkia(it->second));
    return;
  }

  image_view->FetchImage(idp_metadata.brand_icon_url, *image_fetcher_);
}

base::WeakPtr<views::Widget> AccountSelectionViewBase::GetDialogWidget() {
  return dialog_widget_;
}

// static
net::NetworkTrafficAnnotationTag
AccountSelectionViewBase::GetTrafficAnnotation() {
  return kTrafficAnnotation;
}
