// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/groups/avatar_container_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kCircleSize = 20;
constexpr int kIconSize = 12;
constexpr int kTextSize = 12;
constexpr int kCircleOverlap = 4;

class AvatarImageView : public views::ImageView {
  METADATA_HEADER(AvatarImageView, views::ImageView)
  gfx::Size GetImageSize() const override {
    return image_size_.value_or(GetImageModel().Size());
  }
};

BEGIN_METADATA(AvatarImageView)
END_METADATA

void DrawClippedCircle(gfx::Canvas& canvas,
                       int diameter,
                       SkColor circle_color,
                       bool clip_right_of_circle) {
  if (clip_right_of_circle) {
    const SkPath right_clip_circle =
        SkPath::Circle(diameter + (diameter / 3.0f), diameter / 2.0f,
                       diameter / 2.0f)
            .makeFillType(SkPathFillType::kInverseWinding);
    canvas.ClipPath(right_clip_circle, /*do_anti_alias=*/true);
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(circle_color);
  canvas.DrawCircle(gfx::PointF(diameter / 2.0f, diameter / 2.0f),
                    diameter / 2.0f, flags);
}

void DrawFallbackIcon(gfx::Canvas& canvas, int diameter, SkColor icon_color) {
  int icon_offset = (diameter - kIconSize) / 2;
  canvas.Save();
  canvas.Translate({icon_offset, icon_offset});
  gfx::PaintVectorIcon(&canvas, kTabGroupSharingIcon, kIconSize, icon_color);
  canvas.Restore();
}

void DrawNumberText(gfx::Canvas& canvas,
                    int diameter,
                    int number,
                    SkColor text_color,
                    int font_size) {
  const auto& font_list = views::TypographyProvider::Get().GetFont(
      views::style::TextContext::CONTEXT_DIALOG_BODY_TEXT,
      views::style::TextStyle::STYLE_CAPTION_MEDIUM);
  gfx::Rect text_bounds(0, 0, diameter, diameter);
  canvas.DrawStringRectWithFlags(
      base::UTF8ToUTF16("+" + base::NumberToString(number)), font_list,
      text_color, text_bounds, gfx::Canvas::TEXT_ALIGN_CENTER);
}

std::unique_ptr<AvatarImageView> CreateCircleFallbackAvatarImageView(
    int diameter,
    SkColor circle_color,
    SkColor icon_color,
    bool clip_right_of_circle,
    float scale_factor) {
  gfx::Canvas canvas(gfx::Size(diameter, diameter), scale_factor,
                     /*is_opaque=*/false);
  canvas.Save();

  DrawClippedCircle(canvas, diameter, circle_color, clip_right_of_circle);
  DrawFallbackIcon(canvas, diameter, icon_color);

  canvas.Restore();

  gfx::ImageSkia circle_image =
      gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), scale_factor);

  auto image_view = std::make_unique<AvatarImageView>();
  image_view->SetImage(ui::ImageModel::FromImageSkia(circle_image));
  return image_view;
}

std::unique_ptr<AvatarImageView> CreateCircleNumberImageView(
    int number,
    int diameter,
    SkColor circle_color,
    SkColor text_color,
    int font_size,
    bool clip_right_of_circle,
    float scale_factor) {
  gfx::Canvas canvas(gfx::Size(diameter, diameter), scale_factor,
                     /*is_opaque=*/false);
  canvas.Save();

  DrawClippedCircle(canvas, diameter, circle_color, clip_right_of_circle);
  DrawNumberText(canvas, diameter, number, text_color, font_size);

  canvas.Restore();

  gfx::ImageSkia circle_image =
      gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), scale_factor);

  auto image_view = std::make_unique<AvatarImageView>();
  image_view->SetImage(ui::ImageModel::FromImageSkia(circle_image));
  return image_view;
}

std::unique_ptr<AvatarImageView> CreateCircleAvatarImageView(
    const gfx::Image& image,
    int diameter,
    SkColor circle_color,
    SkColor icon_color,
    bool clip_right_of_circle,
    float scale_factor) {
  gfx::ImageSkia image_skia = image.AsImageSkia();
  if (image_skia.isNull()) {
    return CreateCircleFallbackAvatarImageView(
        diameter, circle_color, icon_color, clip_right_of_circle, scale_factor);
  }

  gfx::Canvas canvas(gfx::Size(diameter, diameter), scale_factor,
                     /*is_opaque=*/false);
  canvas.Save();

  const SkPath circle_path =
      SkPath::Circle(diameter / 2.0f, diameter / 2.0f, diameter / 2.0f);
  canvas.ClipPath(circle_path, /*do_anti_alias=*/true);

  const int img_width = image_skia.width();
  const int img_height = image_skia.height();
  float scale = std::max(static_cast<float>(diameter) / img_width,
                         static_cast<float>(diameter) / img_height);

  const int new_width = static_cast<int>(img_width * scale);
  const int new_height = static_cast<int>(img_height * scale);
  const int offset_x = (diameter - new_width) / 2;
  const int offset_y = (diameter - new_height) / 2;

  cc::PaintFlags paint_flags;
  paint_flags.setFilterQuality(cc::PaintFlags::FilterQuality::kHigh);
  paint_flags.setAntiAlias(true);

  canvas.DrawImageInt(image_skia, 0, 0, img_width, img_height, offset_x,
                      offset_y, new_width, new_height,
                      /*filter=*/false, paint_flags);

  canvas.Restore();

  gfx::ImageSkia circle_image =
      gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), scale_factor);

  auto image_view = std::make_unique<AvatarImageView>();
  image_view->SetImage(ui::ImageModel::FromImageSkia(circle_image));
  return image_view;
}

}  // namespace

void ManageSharingAvatarContainer::UpdateMemberGfxImage(
    size_t index,
    const gfx::Image& image) {
  CHECK_LT(index, member_gfx_images_.size());
  member_gfx_images_[index] = image;
  RebuildChildren();
}

ManageSharingAvatarContainer::ManageSharingAvatarContainer(
    Profile* profile,
    const syncer::CollaborationId& collaboration_id)
    : data_sharing_service_(
          data_sharing::DataSharingServiceFactory::GetForProfile(profile)),
      profile_(profile),
      collaboration_id_(collaboration_id) {
  SetLayoutManager(std::make_unique<views::BoxLayout>());
  RequeryMemberInfo();
  RebuildChildren();
}

ManageSharingAvatarContainer::~ManageSharingAvatarContainer() = default;

void ManageSharingAvatarContainer::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  views::View::OnDeviceScaleFactorChanged(old_device_scale_factor,
                                          new_device_scale_factor);
  RequeryMemberInfo();
  RebuildChildren();
}

void ManageSharingAvatarContainer::RequeryMemberInfo() {
  // This action cant be performed if there is no DataSharingService.
  if (!data_sharing_service_) {
    return;
  }

  members_for_display_ =
      tab_groups::SavedTabGroupUtils::GetMembersOfSharedTabGroup(
          profile_, collaboration_id_);

  image_fetcher::ImageFetcherService* image_fetcher_service =
      ImageFetcherServiceFactory::GetForKey(profile_->GetProfileKey());
  image_fetcher::ImageFetcher* image_fetcher =
      image_fetcher_service->GetImageFetcher(
          image_fetcher::ImageFetcherConfig::kDiskCacheOnly);

  // Attempt to get the exact scaled avatar image, default to a overscaled by 2
  // to support HiDPI displays.
  auto image_size = kCircleSize * 2;
  if (GetWidget() && GetWidget()->GetCompositor()) {
    image_size =
        GetWidget()->GetCompositor()->device_scale_factor() * kCircleSize;
  }

  // If we have up to three members, initiate fetch for each.
  if (members_for_display_.size() > 0) {
    data_sharing_service_->GetAvatarImageForURL(
        members_for_display_[0].avatar_url, image_size,
        base::BindOnce(&ManageSharingAvatarContainer::UpdateMemberGfxImage,
                       weak_ptr_factory_.GetWeakPtr(), 0),
        image_fetcher);
  }
  if (members_for_display_.size() > 1) {
    data_sharing_service_->GetAvatarImageForURL(
        members_for_display_[1].avatar_url, image_size,
        base::BindOnce(&ManageSharingAvatarContainer::UpdateMemberGfxImage,
                       weak_ptr_factory_.GetWeakPtr(), 1),
        image_fetcher);
  }
  // Only fetch the 3rd member if we have exactly 3 if there are more than 3 and
  // we show the overflow.
  if (members_for_display_.size() == 3) {
    data_sharing_service_->GetAvatarImageForURL(
        members_for_display_[2].avatar_url, image_size,
        base::BindOnce(&ManageSharingAvatarContainer::UpdateMemberGfxImage,
                       weak_ptr_factory_.GetWeakPtr(), 2),
        image_fetcher);
  }
}

void ManageSharingAvatarContainer::RebuildChildren() {
  member_1_image_view_ = nullptr;
  member_2_image_view_ = nullptr;
  member_3_or_overflow_image_view_ = nullptr;

  RemoveAllChildViews();

  const ui::ColorProvider* color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }

  // Get devices scale factor for scaling the bitmaps.
  float scale_factor = 1.0f;
  if (GetWidget() && GetWidget()->GetCompositor()) {
    scale_factor = GetWidget()->GetCompositor()->device_scale_factor();
  }

  const int member_count = members_for_display_.size();
  const auto circle_color =
      color_provider->GetColor(ui::kColorSysNeutralContainer);
  const auto icon_color = color_provider->GetColor(ui::kColorSysOnSurface);

  // For each of the members, try to add the member image, otherwise fallback
  // the circle with the avatar image.
  if (member_count >= 1) {
    if (member_gfx_images_[0].has_value()) {
      member_1_image_view_ = AddChildView(CreateCircleAvatarImageView(
          member_gfx_images_[0].value(), kCircleSize, circle_color, icon_color,
          /*clip_right_of_circle=*/(member_count > 1), scale_factor));
    } else {
      member_1_image_view_ = AddChildView(CreateCircleFallbackAvatarImageView(
          kCircleSize, circle_color, icon_color,
          /*clip_right_of_circle=*/(member_count > 1), scale_factor));
    }
    member_1_image_view_->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(0, -1 * kCircleOverlap, 0, 0));
  }

  if (member_count >= 2) {
    if (member_gfx_images_[1].has_value()) {
      member_2_image_view_ = AddChildView(CreateCircleAvatarImageView(
          member_gfx_images_[1].value(), kCircleSize, circle_color, icon_color,
          /*clip_right_of_circle=*/(member_count > 2), scale_factor));
    } else {
      member_2_image_view_ = AddChildView(CreateCircleFallbackAvatarImageView(
          kCircleSize, circle_color, icon_color,
          /*clip_right_of_circle=*/(member_count > 2), scale_factor));
    }
    member_2_image_view_->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(0, -1 * kCircleOverlap, 0, 0));
  }

  if (member_count == 3) {
    if (member_gfx_images_[2].has_value()) {
      member_3_or_overflow_image_view_ =
          AddChildView(CreateCircleAvatarImageView(
              member_gfx_images_[2].value(), kCircleSize, circle_color,
              icon_color,
              /*clip_right_of_circle=*/false, scale_factor));
    } else {
      member_3_or_overflow_image_view_ =
          AddChildView(CreateCircleFallbackAvatarImageView(
              kCircleSize, circle_color, icon_color,
              /*clip_right_of_circle=*/false, scale_factor));
    }
  } else if (member_count > 3) {
    member_3_or_overflow_image_view_ = AddChildView(CreateCircleNumberImageView(
        member_count - 2, kCircleSize, circle_color, icon_color, kTextSize,
        /*clip_right_of_circle=*/false, scale_factor));
  }
  if (member_3_or_overflow_image_view_) {
    member_3_or_overflow_image_view_->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(0, -1 * kCircleOverlap, 0, 0));
  }
}

void ManageSharingAvatarContainer::AddedToWidget() {
  views::View::AddedToWidget();
  RebuildChildren();
  SchedulePaint();
}

void ManageSharingAvatarContainer::OnThemeChanged() {
  views::View::OnThemeChanged();
  RebuildChildren();
  SchedulePaint();
}

BEGIN_METADATA(ManageSharingAvatarContainer)
END_METADATA
