// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/move_to_account_store_bubble_view.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kImageSize = 48;

// An image source that adds a circular border and an optional circular
// background to the given image.
class BackgroundBorderAdderImageSource : public gfx::CanvasImageSource {
 public:
  BackgroundBorderAdderImageSource(const gfx::ImageSkia& image,
                                   bool add_background,
                                   std::optional<SkColor> background_color,
                                   SkColor border_color,
                                   int radius)
      : gfx::CanvasImageSource(gfx::Size(radius, radius)),
        image_(image),
        add_background_(add_background),
        background_color_(background_color),
        border_color_(border_color) {}
  ~BackgroundBorderAdderImageSource() override = default;

  void Draw(gfx::Canvas* canvas) override;

 private:
  const gfx::ImageSkia image_;
  const bool add_background_;
  const std::optional<SkColor> background_color_;
  const SkColor border_color_;
};

void BackgroundBorderAdderImageSource::Draw(gfx::Canvas* canvas) {
  constexpr int kBorderThickness = 1;
  float radius = size().width() / 2.0f;
  float half_thickness = kBorderThickness / 2.0f;
  gfx::SizeF size_f(size());
  gfx::RectF bounds(size_f);
  bounds.Inset(half_thickness);
  // Draw the background
  if (add_background_) {
    DCHECK(background_color_);
    cc::PaintFlags background_flags;
    background_flags.setStyle(cc::PaintFlags::kFill_Style);
    background_flags.setAntiAlias(true);
    background_flags.setColor(background_color_.value());
    canvas->DrawRoundRect(bounds, radius, background_flags);
  }
  // Draw the image
  canvas->DrawImageInt(image_, (size().width() - image_.width()) / 2,
                       (size().height() - image_.height()) / 2);
  // Draw the border
  cc::PaintFlags border_flags;
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setAntiAlias(true);
  border_flags.setColor(border_color_);
  canvas->DrawRoundRect(bounds, radius, border_flags);
}

// A class represting an image with a badge. By default, the image is the globe
// icon. However, badge could be updated via the UpdateBadge() method.
class ImageWithBadge : public views::ImageView {
  METADATA_HEADER(ImageWithBadge, views::ImageView)

 public:
  // Constructs a View hierarchy with the a badge positioned in the bottom-right
  // corner of |main_image|. In RTL mode the badge is positioned in the
  // bottom-left corner.
  explicit ImageWithBadge(const gfx::ImageSkia& main_image);
  explicit ImageWithBadge(const gfx::VectorIcon& main_image);
  ~ImageWithBadge() override = default;

  // views::ImageView:
  void OnThemeChanged() override;

  void UpdateBadge(const gfx::ImageSkia& badge_image);

 private:
  gfx::ImageSkia GetMainImage() const;
  gfx::ImageSkia GetBadge() const;
  void Render();

  raw_ptr<const gfx::VectorIcon> main_vector_icon_ = nullptr;
  std::optional<gfx::ImageSkia> main_image_skia_;
  std::optional<gfx::ImageSkia> badge_image_skia_;
};

ImageWithBadge::ImageWithBadge(const gfx::ImageSkia& main_image)
    : main_image_skia_(main_image) {}

ImageWithBadge::ImageWithBadge(const gfx::VectorIcon& main_image)
    : main_vector_icon_(&main_image) {}

void ImageWithBadge::OnThemeChanged() {
  ImageView::OnThemeChanged();
  Render();
}

void ImageWithBadge::UpdateBadge(const gfx::ImageSkia& badge_image) {
  badge_image_skia_ = badge_image;
  Render();
}

gfx::ImageSkia ImageWithBadge::GetMainImage() const {
  if (main_image_skia_) {
    return main_image_skia_.value();
  }
  DCHECK(main_vector_icon_);
  const SkColor color = GetColorProvider()->GetColor(ui::kColorIcon);
  return gfx::CreateVectorIcon(*main_vector_icon_, kImageSize, color);
}

gfx::ImageSkia ImageWithBadge::GetBadge() const {
  if (badge_image_skia_) {
    return badge_image_skia_.value();
  }
  // If there is no badge set, fallback to the default globe icon.
  const SkColor color = GetColorProvider()->GetColor(ui::kColorIcon);
  return gfx::CreateVectorIcon(kGlobeIcon, gfx::kFaviconSize, color);
}

void ImageWithBadge::Render() {
  constexpr int kBadgePadding = 6;
  const auto* color_provider = GetColorProvider();
  const SkColor kBackgroundColor =
      color_provider->GetColor(ui::kColorBubbleBackground);
  const SkColor kBorderColor = color_provider->GetColor(ui::kColorBubbleBorder);

  gfx::Image rounded_badge = profiles::GetSizedAvatarIcon(
      gfx::Image(GetBadge()),
      /*width=*/gfx::kFaviconSize, /*height=*/gfx::kFaviconSize,
      profiles::SHAPE_CIRCLE);

  gfx::ImageSkia rounded_badge_with_background_and_border =
      gfx::CanvasImageSource::MakeImageSkia<BackgroundBorderAdderImageSource>(
          *rounded_badge.ToImageSkia(), /*add_background=*/true,
          kBackgroundColor, kBorderColor, gfx::kFaviconSize + kBadgePadding);

  gfx::ImageSkia main_image_with_border =
      gfx::CanvasImageSource::MakeImageSkia<BackgroundBorderAdderImageSource>(
          GetMainImage(), /*add_background=*/false,
          /*background_color=*/std::nullopt, kBorderColor, kImageSize);

  gfx::ImageSkia badged_image = gfx::ImageSkiaOperations::CreateIconWithBadge(
      main_image_with_border, rounded_badge_with_background_and_border);
  SetImage(ui::ImageModel::FromImageSkia(badged_image));
}

BEGIN_METADATA(ImageWithBadge)
END_METADATA

std::unique_ptr<views::Label> CreateDescription(
    const std::u16string& profile_email) {
  auto description = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_SAVE_IN_ACCOUNT_BUBBLE_DESCRIPTION,
          profile_email),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_HINT);
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return description;
}

}  // namespace

// A view that holds two badged images with an arrow between them to illustrate
// that a password is being moved from the device to the account.
class MoveToAccountStoreBubbleView::MovingBannerView : public views::View {
  METADATA_HEADER(MovingBannerView, views::View)

 public:
  MovingBannerView(std::unique_ptr<ImageWithBadge> from_image,
                   std::unique_ptr<ImageWithBadge> to_image);
  ~MovingBannerView() override = default;

  // Updates the badge in both from and to views to be |favicon|.
  void UpdateFavicon(const gfx::ImageSkia& favicon);

 private:
  raw_ptr<ImageWithBadge> from_view;
  raw_ptr<ImageWithBadge> to_view;
};

MoveToAccountStoreBubbleView::MovingBannerView::MovingBannerView(
    std::unique_ptr<ImageWithBadge> from_image,
    std::unique_ptr<ImageWithBadge> to_image) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  from_view = AddChildView(std::move(from_image));

  auto arrow_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kChevronRightIcon, ui::kColorIcon, gfx::kFaviconSize));
  arrow_view->SetFlipCanvasOnPaintForRTLUI(true);
  AddChildView(std::move(arrow_view));

  to_view = AddChildView(std::move(to_image));
}

void MoveToAccountStoreBubbleView::MovingBannerView::UpdateFavicon(
    const gfx::ImageSkia& favicon) {
  from_view->UpdateBadge(favicon);
  to_view->UpdateBadge(favicon);
}

BEGIN_METADATA(MoveToAccountStoreBubbleView, MovingBannerView)
END_METADATA

MoveToAccountStoreBubbleView::MoveToAccountStoreBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*auto_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded,
                                   /*adjust_height_for_width=*/true))
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_CONTROL_LIST_VERTICAL),
                          0));

  AddChildView(CreateDescription(controller_.GetProfileEmail()));

  auto computer_view =
      std::make_unique<ImageWithBadge>(kHardwareComputerSmallIcon);
  auto avatar_view = std::make_unique<ImageWithBadge>(
      *controller_.GetProfileIcon(kImageSize).ToImageSkia());

  moving_banner_ = AddChildView(std::make_unique<MovingBannerView>(
      /*from_view=*/std::move(computer_view),
      /*to_view=*/std::move(avatar_view)));

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_SAVE_IN_ACCOUNT_BUBBLE_SAVE_BUTTON));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_MOVE_BUBBLE_CANCEL_BUTTON));
  SetAcceptCallback(
      base::BindOnce(&MoveToAccountStoreBubbleController::AcceptMove,
                     base::Unretained(&controller_)));
  SetCancelCallback(
      base::BindOnce(&MoveToAccountStoreBubbleController::RejectMove,
                     base::Unretained(&controller_)));

  SetShowIcon(true);

  // The request is cancelled when the |controller_| is destructed.
  // |controller_| has the same life time as |this| and hence it's safe to use
  // base::Unretained(this).
  controller_.RequestFavicon(base::BindOnce(
      &MoveToAccountStoreBubbleView::OnFaviconReady, base::Unretained(this)));
}

MoveToAccountStoreBubbleView::~MoveToAccountStoreBubbleView() = default;

void MoveToAccountStoreBubbleView::AddedToWidget() {
  static_cast<views::Label*>(GetBubbleFrameView()->title())
      ->SetAllowCharacterBreak(true);
}

MoveToAccountStoreBubbleController*
MoveToAccountStoreBubbleView::GetController() {
  return &controller_;
}

const MoveToAccountStoreBubbleController*
MoveToAccountStoreBubbleView::GetController() const {
  return &controller_;
}

ui::ImageModel MoveToAccountStoreBubbleView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void MoveToAccountStoreBubbleView::OnFaviconReady(const gfx::Image& favicon) {
  if (!favicon.IsEmpty()) {
    moving_banner_->UpdateFavicon(*favicon.ToImageSkia());
  }
}

BEGIN_METADATA(MoveToAccountStoreBubbleView)
END_METADATA
