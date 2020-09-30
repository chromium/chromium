// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/move_to_account_store_bubble_view.h"
#include <algorithm>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

// The space between the right/bottom edge of the badge and the
// right/bottom edge of the main icon.
constexpr int kBadgeSpacing = 4;
constexpr int kBadgeBorderWidth = 2;
constexpr int kImageSize = BadgedProfilePhoto::kImageSize;
// Width and Height of the badged icon.
constexpr int kBadgedProfilePhotoSize = kImageSize + kBadgeSpacing;

// An images view with an empty space for the badge.
class ImageViewWithPlaceForBadge : public views::ImageView {
  // views::ImageView
  void OnPaint(gfx::Canvas* canvas) override {
    const int kBadgeIconSize = gfx::kFaviconSize;
    // Remove the part of the ImageView that contains the badge.
    SkPath mask;
    mask.addCircle(
        /*x=*/GetMirroredXInView(kBadgedProfilePhotoSize - kBadgeIconSize / 2),
        /*y=*/kBadgedProfilePhotoSize - kBadgeIconSize / 2,
        /*radius=*/kBadgeIconSize / 2 + kBadgeBorderWidth);
    mask.toggleInverseFillType();
    canvas->ClipPath(mask, true);
    ImageView::OnPaint(canvas);
  }
};

// An image view that shows a vector icon and tracks changes in the theme.
class VectorIconView : public ImageViewWithPlaceForBadge {
 public:
  explicit VectorIconView(const gfx::VectorIcon& icon, int size)
      : icon_(icon), size_(size) {}

  // views::ImageView
  void OnThemeChanged() override {
    ImageView::OnThemeChanged();
    const SkColor color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_DefaultIconColor);
    SetImage(gfx::CreateVectorIcon(icon_, size_, color));
    SizeToPreferredSize();
  }

 private:
  const gfx::VectorIcon& icon_;
  const int size_;
};

// A class represting an image with a badge. By default, the image is the globe
// icon. However, badge could be updated via the UpdateBadge() method.
class ImageWithBadge : public views::View {
 public:
  // Constructs a View hierarchy with the a badge positioned in the bottom-right
  // corner of |main_image|. In RTL mode the badge is positioned in the
  // bottom-left corner.
  explicit ImageWithBadge(const gfx::ImageSkia& main_image);
  explicit ImageWithBadge(const gfx::VectorIcon& main_image);
  ~ImageWithBadge() override = default;

  void UpdateBadge(const gfx::ImageSkia& badge_image);

 private:
  // Adds a default badge of "globe" icon.
  void AddDefaultBadge();

  views::ImageView* badge_view_;
};

ImageWithBadge::ImageWithBadge(const gfx::ImageSkia& main_image) {
  SetCanProcessEventsWithinSubtree(false);
  auto main_view = std::make_unique<ImageViewWithPlaceForBadge>();
  main_view->SetImage(main_image);
  main_view->SizeToPreferredSize();
  AddChildView(std::move(main_view));
  AddDefaultBadge();
}

ImageWithBadge::ImageWithBadge(const gfx::VectorIcon& main_image) {
  SetCanProcessEventsWithinSubtree(false);
  auto main_view = std::make_unique<VectorIconView>(main_image, kImageSize);
  main_view->SizeToPreferredSize();
  AddChildView(std::move(main_view));
  AddDefaultBadge();
}

void ImageWithBadge::AddDefaultBadge() {
  const int kBadgeIconSize = gfx::kFaviconSize;
  // Use a Globe icon as the default badge.
  auto badge_view =
      std::make_unique<VectorIconView>(kGlobeIcon, kBadgeIconSize);
  badge_view->SetPosition(gfx::Point(kBadgedProfilePhotoSize - kBadgeIconSize,
                                     kBadgedProfilePhotoSize - kBadgeIconSize));
  badge_view->SizeToPreferredSize();
  badge_view_ = AddChildView(std::move(badge_view));

  SetPreferredSize(gfx::Size(kBadgedProfilePhotoSize, kBadgedProfilePhotoSize));
}

void ImageWithBadge::UpdateBadge(const gfx::ImageSkia& badge_image) {
  gfx::Image rounded_badge = profiles::GetSizedAvatarIcon(
      gfx::Image(badge_image),
      /*is_rectangle=*/true, /*width=*/gfx::kFaviconSize,
      /*height=*/gfx::kFaviconSize, profiles::SHAPE_CIRCLE);
  badge_view_->SetImage(rounded_badge.ToImageSkia());
  badge_view_->SizeToPreferredSize();
}

std::unique_ptr<views::View> CreateHeaderImage(int image_id) {
  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetImage(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id));
  gfx::Size preferred_size = image_view->GetPreferredSize();
  if (preferred_size.width()) {
    preferred_size = gfx::ScaleToRoundedSize(
        preferred_size,
        static_cast<float>(ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_BUBBLE_PREFERRED_WIDTH)) /
            preferred_size.width());
    image_view->SetImageSize(preferred_size);
  }
  return image_view;
}

std::unique_ptr<views::Label> CreateDescription() {
  auto description = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_HINT),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_HINT);
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return description;
}

}  // namespace

// A view that holds two badged images with an arrow between them to illustrate
// that a password is being moved from the device to the account.
class MoveToAccountStoreBubbleView::MovingBannerView : public views::View {
 public:
  MovingBannerView(std::unique_ptr<ImageWithBadge> from_image,
                   std::unique_ptr<ImageWithBadge> to_image);
  ~MovingBannerView() override = default;

  // Updates the badge in both from and to views to be |favicon|.
  void UpdateFavicon(const gfx::ImageSkia& favicon);

 private:
  ImageWithBadge* from_view;
  ImageWithBadge* to_view;
};

MoveToAccountStoreBubbleView::MovingBannerView::MovingBannerView(
    std::unique_ptr<ImageWithBadge> from_image,
    std::unique_ptr<ImageWithBadge> to_image) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  from_view = AddChildView(std::move(from_image));

  auto arrow_view = std::make_unique<VectorIconView>(
      kBookmarkbarTouchOverflowIcon, kImageSize);
  arrow_view->EnableCanvasFlippingForRTLUI(true);
  AddChildView(std::move(arrow_view));

  to_view = AddChildView(std::move(to_image));
}

void MoveToAccountStoreBubbleView::MovingBannerView::UpdateFavicon(
    const gfx::ImageSkia& favicon) {
  from_view->UpdateBadge(favicon);
  to_view->UpdateBadge(favicon);
}

MoveToAccountStoreBubbleView::MoveToAccountStoreBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*auto_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

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
          gfx::Insets(
              /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL),
              /*horizontal=*/0));

  AddChildView(CreateDescription());

  auto computer_view =
      std::make_unique<ImageWithBadge>(kComputerWithCircleBackgroundIcon);
  auto avatar_view = std::make_unique<ImageWithBadge>(
      *controller_.GetProfileIcon(kImageSize).ToImageSkia());

  moving_banner_ = AddChildView(std::make_unique<MovingBannerView>(
      /*from_view=*/std::move(computer_view),
      /*to_view=*/std::move(avatar_view)));

  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_BUBBLE_OK_BUTTON));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_MOVE_BUBBLE_CANCEL_BUTTON));
  SetAcceptCallback(
      base::BindOnce(&MoveToAccountStoreBubbleController::AcceptMove,
                     base::Unretained(&controller_)));
  SetCancelCallback(
      base::BindOnce(&MoveToAccountStoreBubbleController::RejectMove,
                     base::Unretained(&controller_)));

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

void MoveToAccountStoreBubbleView::OnThemeChanged() {
  PasswordBubbleViewBase::OnThemeChanged();
  GetBubbleFrameView()->SetHeaderView(CreateHeaderImage(
      color_utils::IsDark(GetBubbleFrameView()->GetBackgroundColor())
          ? IDR_SAVE_PASSWORD_MULTI_DEVICE_DARK
          : IDR_SAVE_PASSWORD_MULTI_DEVICE));
}

gfx::Size MoveToAccountStoreBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

bool MoveToAccountStoreBubbleView::ShouldShowCloseButton() const {
  return true;
}

MoveToAccountStoreBubbleController*
MoveToAccountStoreBubbleView::GetController() {
  return &controller_;
}

const MoveToAccountStoreBubbleController*
MoveToAccountStoreBubbleView::GetController() const {
  return &controller_;
}

void MoveToAccountStoreBubbleView::OnFaviconReady(const gfx::Image& favicon) {
  if (!favicon.IsEmpty()) {
    moving_banner_->UpdateFavicon(*favicon.ToImageSkia());
  }
}
