// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/chapter_item_view.h"

#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/global_media_controls/media_view_utils.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"

namespace global_media_controls {

namespace {
constexpr auto kItemCornerRadius =
    gfx::RoundedCornersF(kDefaultArtworkCornerRadius);
constexpr gfx::Size kImageSize(64, 40);

// A `HighlightPathGenerator` that uses caller-supplied rounded rect corners.
class RoundedCornerHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  explicit RoundedCornerHighlightPathGenerator(
      const gfx::RoundedCornersF& corners)
      : corners_(corners) {}

  RoundedCornerHighlightPathGenerator(
      const RoundedCornerHighlightPathGenerator&) = delete;
  RoundedCornerHighlightPathGenerator& operator=(
      const RoundedCornerHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    return gfx::RRectF(rect, corners_);
  }

 private:
  // The user-supplied rounded rect corners.
  const gfx::RoundedCornersF corners_;
};
}  // namespace

// TODO(b/327508008): Polish paddings, a11y, color and etc.
ChapterItemView::ChapterItemView(
    const media_session::ChapterInformation& chapter,
    const media_message_center::MediaColorTheme& theme,
    base::RepeatingCallback<void(const base::TimeDelta time)>
        on_chapter_pressed)
    : chapter_(chapter),
      theme_(theme),
      on_chapter_pressed_callback_(std::move(on_chapter_pressed)) {
  SetCallback(base::BindRepeating(&ChapterItemView::PerformAction,
                                  base::Unretained(this)));

  views::Builder<views::View>(this)
      .SetUseDefaultFillLayout(true)
      .SetAccessibleRole(ax::mojom::Role::kButton)
      .SetAccessibleName(u"TBD")
      .SetFocusBehavior(FocusBehavior::ALWAYS)
      .SetPaintToLayer()
      .AddChildren(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetInsideBorderInsets(gfx::Insets(8))
              .SetBetweenChildSpacing(8)
              .AddChildren(
                  views::Builder<views::ImageView>()
                      .CopyAddressTo(&artwork_view_)
                      .SetPreferredSize(kImageSize),
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(views::BoxLayout::Orientation::kVertical)
                      .AddChildren(
                          views::Builder<views::Label>()
                              .SetText(chapter.title())
                              .SetID(kChapterItemViewTitleId)
                              .SetFontList(gfx::FontList(
                                  {"Google Sans"}, gfx::Font::NORMAL, 13,
                                  gfx::Font::Weight::NORMAL))
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                              .SetEnabledColorId(
                                  theme_.primary_foreground_color_id),
                          views::Builder<views::Label>()
                              .SetText(
                                  GetFormattedDuration(chapter.startTime()))
                              .SetID(kChapterItemViewStartTimeId)
                              .SetFontList(gfx::FontList(
                                  {"Google Sans"}, gfx::Font::NORMAL, 12,
                                  gfx::Font::Weight::NORMAL))
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                              .SetEnabledColorId(
                                  theme_.secondary_foreground_color_id))))
      .BuildChildren();

  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(kItemCornerRadius);
  SetUpFocusHighlight(kItemCornerRadius);
}

ChapterItemView::~ChapterItemView() = default;

void ChapterItemView::UpdateArtwork(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Hide the image so the other contents will adjust to fill the container.
    artwork_view_->SetVisible(false);
  }

  // Rescales the image.
  artwork_view_->SetImageSize(
      ScaleImageSizeToFitView(image.size(), kImageSize));
  artwork_view_->SetImage(ui::ImageModel::FromImageSkia(image));

  // Draws the image with rounded corners.
  auto path = SkPath().addRoundRect(
      RectToSkRect(gfx::Rect(kImageSize.width(), kImageSize.height())),
      kDefaultArtworkCornerRadius, kDefaultArtworkCornerRadius);
  artwork_view_->SetClipPath(path);

  artwork_view_->SetVisible(true);
}

void ChapterItemView::PerformAction(const ui::Event& event) {
  on_chapter_pressed_callback_.Run(chapter_.startTime());
}

void ChapterItemView::SetUpFocusHighlight(
    const gfx::RoundedCornersF& item_corner_radius) {
  views::FocusRing::Get(this)->SetColorId(theme_.focus_ring_color_id);
  views::FocusRing::Get(this)->SetHaloThickness(3.0f);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<RoundedCornerHighlightPathGenerator>(
                item_corner_radius));
}

BEGIN_METADATA(ChapterItemView);
END_METADATA

}  // namespace global_media_controls
