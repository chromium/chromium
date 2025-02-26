// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_view_base.h"

#include <memory>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/listformatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace webid {
namespace {

// safe_zone_diameter/icon_size as defined in
// https://www.w3.org/TR/appmanifest/#icon-masks
constexpr float kMaskableWebIconSafeZoneRatio = 0.8f;

// The opacity of the avatar when the account is filtered out.
constexpr double kDisabledAvatarOpacity = 0.38;

// The border radius of the background circle containing the IDP icon in an
// account button.
constexpr int kIdpBorderRadius = 10;

// Error codes.
constexpr char kInvalidRequest[] = "invalid_request";
constexpr char kUnauthorizedClient[] = "unauthorized_client";
constexpr char kAccessDenied[] = "access_denied";
constexpr char kTemporarilyUnavailable[] = "temporarily_unavailable";
constexpr char kServerError[] = "server_error";

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

std::u16string ListToString(base::span<std::u16string> items) {
  std::vector<icu::UnicodeString> strings;
  strings.reserve(items.size());
  for (const auto& item : items) {
    strings.emplace_back(item.data(), item.size());
  }
  UErrorCode error = U_ZERO_ERROR;
  auto formatter = base::WrapUnique(icu::ListFormatter::createInstance(error));
  if (U_FAILURE(error) || !formatter) {
    // Verify that this doesn't happen in practice.
    base::debug::DumpWithoutCrashing();
    return std::u16string();
  }
  icu::UnicodeString formatted;
  formatter->format(strings.data(), strings.size(), formatted, error);
  if (U_FAILURE(error)) {
    // Verify that this doesn't happen in practice.
    base::debug::DumpWithoutCrashing();
    return std::u16string();
  }
  return base::i18n::UnicodeStringToString16(formatted);
}

std::u16string GetPermissionFieldsString(
    const std::vector<content::IdentityRequestDialogDisclosureField>& fields) {
  std::vector<std::u16string> strings;
  for (auto field : fields) {
    switch (field) {
      case content::IdentityRequestDialogDisclosureField::kName:
        strings.push_back(
            l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_DATA_SHARING_NAME));
        break;
      case content::IdentityRequestDialogDisclosureField::kEmail:
        strings.push_back(l10n_util::GetStringUTF16(
            IDS_ACCOUNT_SELECTION_DATA_SHARING_EMAIL));
        break;
      case content::IdentityRequestDialogDisclosureField::kPicture:
        strings.push_back(l10n_util::GetStringUTF16(
            IDS_ACCOUNT_SELECTION_DATA_SHARING_PICTURE));
        break;
      case content::IdentityRequestDialogDisclosureField::kPhoneNumber:
        strings.push_back(l10n_util::GetStringUTF16(
            IDS_ACCOUNT_SELECTION_DATA_SHARING_PHONE));
        break;
      case content::IdentityRequestDialogDisclosureField::kUsername:
        strings.push_back(l10n_util::GetStringUTF16(
            IDS_ACCOUNT_SELECTION_DATA_SHARING_USERNAME));
        break;
    }
  }
  return ListToString(strings);
}

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

// Returns an image consisting of `base_image` with `badge_image` being badged
// towards its bottom right corner. `badge_offset` is used to determine how much
// bigger the badged image should be with respect to the base image. A
// transparent circular circle is cut out from the bottom right corner of the
// output image, of size `badge_radius`. The following are prerequisites for
// invoking this method:
// * `base_image` and `badge_image` need to be square images.
// * `badge_radius` needs to be at least half of the width of `badge_image`.
//    That is, the diameter of the transparent cutout needs to be larger than
//    the size of `badge_image`.
gfx::ImageSkia CreateBadgedImageSkia(const gfx::ImageSkia& base_image,
                                     const gfx::ImageSkia& badge_image,
                                     int badge_offset,
                                     int badge_radius) {
  // Get the underlying SkBitmaps.
  const SkBitmap* base_bitmap = base_image.bitmap();
  const SkBitmap* badge_bitmap = badge_image.bitmap();

  DCHECK_EQ(base_image.width(), base_image.height());
  DCHECK_EQ(badge_image.width(), badge_image.height());

  int base_size = base_image.width();
  int badge_size = badge_image.width();

  SkBitmap result_bitmap;
  int total_size = base_size + badge_offset;
  result_bitmap.allocN32Pixels(total_size, total_size);

  SkCanvas canvas(result_bitmap);
  canvas.drawImage(base_bitmap->asImage(), 0, 0);

  // Calculate badge position.
  int badge_diameter = badge_radius * 2;
  int badge_outer = badge_diameter - badge_size;
  CHECK_GE(badge_outer, 0);
  int last_position = total_size - 1;
  SkScalar badge_start = last_position - badge_diameter + badge_outer / 2.0f;

  // Create a paint for "punching out" the background.
  SkPaint clear_paint;
  clear_paint.setAntiAlias(true);
  clear_paint.setBlendMode(SkBlendMode::kDstOut);

  // Calculate badge center position. We'll use a center for the circle.
  SkScalar badge_center = last_position - badge_radius;

  // "Punch out" the area around the badge, then draw the badge.
  canvas.drawCircle(badge_center, badge_center, badge_radius, clear_paint);
  canvas.drawImage(badge_bitmap->asImage(), badge_start, badge_start);

  return gfx::ImageSkia::CreateFrom1xBitmap(result_bitmap);
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
                       int image_size,
                       std::optional<gfx::ImageSkia> idp_image = std::nullopt) {
    if (account.decoded_picture.IsEmpty()) {
      std::u16string letter =
          AccountSelectionViewBase::GetInitialLetterAsUppercase(account.name);
      avatar_ = gfx::CanvasImageSource::MakeImageSkia<
          LetterCircleCroppedImageSkiaSource>(letter, image_size);
    } else {
      avatar_ =
          gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
              account.decoded_picture.AsImageSkia(), std::nullopt, image_size);
    }
    if (account.is_filtered_out) {
      avatar_ = gfx::ImageSkiaOperations::CreateTransparentImage(
          avatar_, kDisabledAvatarOpacity);
    }
    if (idp_image && idp_image->width() == idp_image->height() &&
        idp_image->width() >=
            kLargeAvatarBadgeSize / kMaskableWebIconSafeZoneRatio) {
      gfx::ImageSkia cropped_idp_image =
          CreateCircleCroppedImage(*idp_image, kLargeAvatarBadgeSize);
      avatar_ = CreateBadgedImageSkia(avatar_, cropped_idp_image,
                                      kIdpBadgeOffset, kIdpBorderRadius);
    }
    SetImage(ui::ImageModel::FromImageSkia(avatar_));
  }

  void SetDisabledOpacity() {
    avatar_ = gfx::ImageSkiaOperations::CreateTransparentImage(
        avatar_, kDisabledAvatarOpacity);
    SetImage(ui::ImageModel::FromImageSkia(avatar_));
  }

 private:
  gfx::ImageSkia avatar_;
  base::WeakPtrFactory<AccountImageView> weak_ptr_factory_{this};
};

BEGIN_METADATA(AccountImageView)
END_METADATA

}  // namespace

AccountHoverButtonSecondaryView::AccountHoverButtonSecondaryView() {
  constexpr int kSecondaryViewRightPadding = 8;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(/*top=*/0, /*left=*/0, /*bottom=*/0,
                        /*right=*/kSecondaryViewRightPadding)));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  std::unique_ptr<views::ImageView> arrow_image_view =
      std::make_unique<views::ImageView>();
  arrow_image_view->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kSubmenuArrowIcon, ui::kColorIcon, kArrowIconSize));
  arrow_image_view_ = AddChildView(std::move(arrow_image_view));
}

void AccountHoverButtonSecondaryView::ReplaceWithSpinner() {
  std::unique_ptr<views::Throbber> spinner =
      std::make_unique<views::Throbber>();
  constexpr int kSpinnerSize = 24;
  spinner->SetPreferredSize(gfx::Size(kSpinnerSize, kSpinnerSize));
  spinner->Start();
  arrow_image_view_ = nullptr;
  RemoveAllChildViews();
  AddChildView(std::move(spinner));
}

void AccountHoverButtonSecondaryView::SetDisabledOpacity() {
  if (!arrow_image_view_) {
    return;
  }

  arrow_image_view_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kSubmenuArrowIcon, ui::kColorLabelForegroundDisabled,
      kArrowIconSize));
}

BrandIconImageView::BrandIconImageView(int image_size,
                                       bool should_circle_crop,
                                       base::RepeatingClosure on_image_set)
    : image_size_(image_size),
      should_circle_crop_(should_circle_crop),
      on_image_set_(std::move(on_image_set)) {}

BrandIconImageView::~BrandIconImageView() = default;

void BrandIconImageView::CropAndSetImage(const gfx::Image& image) {
  if (image.Width() != image.Height() ||
      image.Width() < (image_size_ / kMaskableWebIconSafeZoneRatio)) {
    return;
  }
  const gfx::ImageSkia& original_image = image.AsImageSkia();
  gfx::ImageSkia cropped_idp_image =
      should_circle_crop_
          ? CreateCircleCroppedImage(original_image, image_size_)
          : gfx::ImageSkiaOperations::CreateResizedImage(
                original_image, skia::ImageOperations::RESIZE_BEST,
                gfx::Size(image_size_, image_size_));
  SetImage(ui::ImageModel::FromImageSkia(cropped_idp_image));

  if (!on_image_set_) {
    return;
  }
  std::move(on_image_set_).Run();
}

BEGIN_METADATA(BrandIconImageView)
END_METADATA

AccountHoverButton::AccountHoverButton(
    PressedCallback callback,
    std::unique_ptr<views::View> icon_view,
    const std::u16string& title,
    const std::u16string& subtitle,
    std::unique_ptr<views::View> secondary_view,
    bool add_vertical_label_spacing,
    const std::u16string& footer,
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
      button_position_(button_position) {}

void AccountHoverButton::OnPressed(const ui::Event& event) {
  // We do not disable the button which has been clicked because otherwise,
  // focus wouldn't be able to remain on the selected account row and causes the
  // focus to move to the cancel button. Since the button is not disabled, it is
  // possible for the button to be clicked again and we would ignore these
  // future clicks.
  if (has_been_clicked_) {
    return;
  }

  // Log the metric before invoking the callback since the callback may
  // destroy this object.
  base::UmaHistogramCustomCounts("Blink.FedCm.AccountChosenPosition.Desktop",
                                 button_position_,
                                 /*min=*/0,
                                 /*exclusive_max=*/10, /*buckets=*/11);
  has_been_clicked_ = true;
  if (callback_) {
    callback_.Run(event);
  }
}

bool AccountHoverButton::HasBeenClicked() {
  return has_been_clicked_;
}

void AccountHoverButton::SetDisabledOpacity() {
  is_appear_disabled_ = true;

  if (has_spinner_) {
    return;
  }

  if (icon_view()) {
    static_cast<AccountImageView*>(icon_view())->SetDisabledOpacity();
  }

  if (secondary_view()) {
    static_cast<AccountHoverButtonSecondaryView*>(secondary_view())
        ->SetDisabledOpacity();
  }

  title()->SetDefaultEnabledColorId(ui::kColorLabelForegroundDisabled);
  subtitle()->SetEnabledColor(ui::kColorLabelForegroundDisabled);

  // Recreates the StyledLabel with the new default enabled color id.
  title()->PreferredSizeChanged();
}

bool AccountHoverButton::HasDisabledOpacity() {
  return is_appear_disabled_;
}

void AccountHoverButton::ReplaceSecondaryViewWithSpinner() {
  has_spinner_ = true;
  static_cast<AccountHoverButtonSecondaryView*>(secondary_view())
      ->ReplaceWithSpinner();
}

AccountSelectionViewBase::AccountSelectionViewBase(
    FedCmAccountSelectionView* owner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::u16string rp_for_display)
    : owner_(owner), rp_for_display_(rp_for_display) {}

AccountSelectionViewBase::~AccountSelectionViewBase() = default;

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

/* static */ std::u16string
AccountSelectionViewBase::GetInitialLetterAsUppercase(
    const std::string& utf8_string) {
  std::u16string utf16_string(base::UTF8ToUTF16(utf8_string));
  base::i18n::BreakIterator iter(utf16_string,
                                 base::i18n::BreakIterator::BREAK_CHARACTER);
  if (!iter.Init()) {
    return u"";
  }

  if (!iter.Advance()) {
    return u"";
  }

  return base::i18n::ToUpper(iter.GetString());
}

std::unique_ptr<views::View> AccountSelectionViewBase::CreateAccountRow(
    const IdentityRequestAccountPtr& account,
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
  if (account->is_filtered_out) {
    account_name_style = views::style::STYLE_DISABLED;
    account_email_style = views::style::STYLE_DISABLED;
  }

  auto account_image_view = std::make_unique<AccountImageView>();
  account_image_view->SetImageSize({avatar_size, avatar_size});
  CHECK(clickable_position || !should_include_idp);
  const content::IdentityProviderData& idp_data = *account->identity_provider;
  if (clickable_position) {
    if (should_include_idp) {
      account_image_view->SetImageSize(
          {avatar_size + kIdpBadgeOffset, avatar_size + kIdpBadgeOffset});
      account_image_view->SetAccountImage(
          *account, avatar_size,
          std::make_optional<gfx::ImageSkia>(
              idp_data.idp_metadata.brand_decoded_icon.AsImageSkia()));
    } else {
      account_image_view->SetAccountImage(*account, avatar_size);
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
        base::BindRepeating(&FedCmAccountSelectionView::OnAccountSelected,
                            base::Unretained(owner_), account),
        std::move(account_image_view),
        /*title=*/account->is_filtered_out ? base::UTF8ToUTF16(account->email)
                                           : base::UTF8ToUTF16(account->name),
        /*subtitle=*/account->is_filtered_out
            ? l10n_util::GetStringUTF16(IDS_FILTERED_ACCOUNT_MESSAGE)
            : base::UTF8ToUTF16(account->email),
        /*secondary_view=*/
        is_modal_dialog ? std::make_unique<AccountHoverButtonSecondaryView>()
                        : nullptr,
        /*add_vertical_label_spacing=*/true, footer, *clickable_position);
    row->SetProperty(views::kElementIdentifierKey,
                     kFedCmAccountChooserDialogAccountElementId);

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
    if (account->is_filtered_out) {
      row->SetEnabled(false);
    }
    return row;
  }
  // We should only create non-button account rows for valid accounts.
  CHECK(!account->is_filtered_out);
  account_image_view->SetAccountImage(*account, avatar_size);
  auto row = std::make_unique<views::View>();
  row->SetProperty(views::kElementIdentifierKey,
                   kFedCmAccountChooserDialogAccountElementId);
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
  account_name->SetText(base::UTF8ToUTF16(account->name));
  account_name->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Add account email.
  views::Label* const account_email =
      text_column->AddChildView(std::make_unique<views::Label>(
          base::UTF8ToUTF16(account->email),
          views::style::CONTEXT_DIALOG_BODY_TEXT, account_email_style));
  account_email->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  return row;
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
            &FedCmAccountSelectionView::OnLinkClicked, base::Unretained(owner_),
            link_data_item.first, link_data_item.second)));
    offset_index += 2;
  }

  return disclosure_label;
}

std::pair<std::u16string, std::u16string>
AccountSelectionViewBase::GetErrorDialogText(
    const std::optional<TokenError>& error,
    const std::u16string& rp_for_display,
    const std::u16string& idp_for_display) {
  std::string code = error ? error->code : "";
  GURL url = error ? error->url : GURL();

  std::u16string summary;
  std::u16string description;

  if (code == kInvalidRequest) {
    summary = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_INVALID_REQUEST_ERROR_DIALOG_SUMMARY, rp_for_display,
        idp_for_display);
    description = l10n_util::GetStringUTF16(
        IDS_SIGNIN_INVALID_REQUEST_ERROR_DIALOG_DESCRIPTION);
  } else if (code == kUnauthorizedClient) {
    summary = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_UNAUTHORIZED_CLIENT_ERROR_DIALOG_SUMMARY, rp_for_display,
        idp_for_display);
    description = l10n_util::GetStringUTF16(
        IDS_SIGNIN_UNAUTHORIZED_CLIENT_ERROR_DIALOG_DESCRIPTION);
  } else if (code == kAccessDenied) {
    summary = l10n_util::GetStringUTF16(
        IDS_SIGNIN_ACCESS_DENIED_ERROR_DIALOG_SUMMARY);
    description = l10n_util::GetStringUTF16(
        IDS_SIGNIN_ACCESS_DENIED_ERROR_DIALOG_DESCRIPTION);
  } else if (code == kTemporarilyUnavailable) {
    summary = l10n_util::GetStringUTF16(
        IDS_SIGNIN_TEMPORARILY_UNAVAILABLE_ERROR_DIALOG_SUMMARY);
    description = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_TEMPORARILY_UNAVAILABLE_ERROR_DIALOG_DESCRIPTION,
        idp_for_display);
  } else if (code == kServerError) {
    summary = l10n_util::GetStringUTF16(IDS_SIGNIN_SERVER_ERROR_DIALOG_SUMMARY);
    description = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_SERVER_ERROR_DIALOG_DESCRIPTION, rp_for_display);
    // Extra description is not needed for kServerError.
    return {summary, description};
  } else {
    summary = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_GENERIC_ERROR_DIALOG_SUMMARY, idp_for_display);
    description =
        l10n_util::GetStringUTF16(IDS_SIGNIN_GENERIC_ERROR_DIALOG_DESCRIPTION);
    // Extra description is not needed for the generic error dialog.
    return {summary, description};
  }

  if (url.is_empty()) {
    description +=
        u" " + l10n_util::GetStringFUTF16(
                   code == kTemporarilyUnavailable
                       ? IDS_SIGNIN_ERROR_DIALOG_TRY_OTHER_WAYS_RETRY_PROMPT
                       : IDS_SIGNIN_ERROR_DIALOG_TRY_OTHER_WAYS_PROMPT,
                   rp_for_display);
    return {summary, description};
  }

  description +=
      u" " + l10n_util::GetStringFUTF16(
                 code == kTemporarilyUnavailable
                     ? IDS_SIGNIN_ERROR_DIALOG_MORE_DETAILS_RETRY_PROMPT
                     : IDS_SIGNIN_ERROR_DIALOG_MORE_DETAILS_PROMPT,
                 idp_for_display);
  return {summary, description};
}

}  // namespace webid
