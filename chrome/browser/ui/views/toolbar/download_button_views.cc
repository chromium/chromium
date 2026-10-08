// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/download_button_views.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/progress_ring_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

using GetBadgeTextCallback = base::RepeatingCallback<gfx::RenderText&()>;

constexpr int kProgressRingRadius = 9;
constexpr int kProgressRingRadiusTouchMode = 12;
constexpr float kProgressRingStrokeWidth = 2.0f;

class DownloadProgressRing : public views::View, gfx::AnimationDelegate {
  METADATA_HEADER(DownloadProgressRing, views::View)
 public:
  DownloadProgressRing(DownloadProgressRing&) = delete;
  DownloadProgressRing& operator=(const DownloadProgressRing&) = delete;
  ~DownloadProgressRing() override = default;

  // Returns the progress ring if it is a direct child of `parent`, otherwise
  // creates one and adds it to `parent`. The returned progress ring is owned by
  // `parent`.
  static DownloadProgressRing& GetOrInstall(views::View& parent) {
    for (auto& child : parent.children()) {
      if (views::IsViewClass<DownloadProgressRing>(child)) {
        return *views::AsViewClass<DownloadProgressRing>(child);
      }
    }
    return *parent.AddChildView(
        base::WrapUnique<DownloadProgressRing>(new DownloadProgressRing()));
  }

  void SetIdle() {
    status_ = ActionItemProgressRingStatus::kIdle;
    scanning_animation_.End();
    SchedulePaint();
  }

  void SetDormant() {
    status_ = ActionItemProgressRingStatus::kDormant;
    scanning_animation_.End();
    SchedulePaint();
  }

  void SetScanning() {
    status_ = ActionItemProgressRingStatus::kScanning;
    scanning_animation_.Show();
    SchedulePaint();
  }

  void SetDownloading(int progress_percentage) {
    status_ = ActionItemProgressRingStatus::kDownloading;
    download_progress_percentage_ = progress_percentage;
    scanning_animation_.End();
    SchedulePaint();
  }

  ActionItemProgressRingStatus GetStatus() const { return status_; }

  void UpdateColors(SkColor background_color, SkColor progress_color) {
    background_color_ = background_color;
    progress_color_ = progress_color;
    SchedulePaint();
  }

 private:
  DownloadProgressRing() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    // Don't allow the view to process events.
    SetCanProcessEventsWithinSubtree(false);
    scanning_animation_.SetSlideDuration(base::Milliseconds(2500));
    scanning_animation_.SetTweenType(gfx::Tween::LINEAR);
  }

  // AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    SchedulePaint();
  }

  // View:
  void Layout(PassKey) override {
    LayoutSuperclass<views::View>(this);
    // Fill the parent completely.
    SetBoundsRect(parent()->GetLocalBounds());
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // Do not show the progress ring when there is no in progress download.
    if (status_ == ActionItemProgressRingStatus::kIdle) {
      return;
    }

    int ring_radius = ui::TouchUiController::Get()->touch_ui()
                          ? kProgressRingRadiusTouchMode
                          : kProgressRingRadius;
    int x = width() / 2 - ring_radius;
    int y = height() / 2 - ring_radius;
    int diameter = 2 * ring_radius;
    gfx::RectF ring_bounds(x, y, /*width=*/diameter, /*height=*/diameter);

    if (status_ == ActionItemProgressRingStatus::kDormant) {
      // Draw a static solid ring.
      views::DrawProgressRing(canvas, gfx::RectFToSkRect(ring_bounds),
                              background_color_, background_color_,
                              kProgressRingStrokeWidth,
                              /*start_angle=*/0,
                              /*sweep_angle=*/0);
      return;
    }

    if (status_ == ActionItemProgressRingStatus::kScanning) {
      if (!scanning_animation_.is_animating()) {
        scanning_animation_.Reset();
        scanning_animation_.Show();
      }
      views::DrawSpinningRing(
          canvas, gfx::RectFToSkRect(ring_bounds), background_color_,
          progress_color_, kProgressRingStrokeWidth, /*start_angle=*/
          gfx::Tween::IntValueBetween(scanning_animation_.GetCurrentValue(), 0,
                                      360));
      return;
    }

    if (status_ == ActionItemProgressRingStatus::kDownloading) {
      views::DrawProgressRing(
          canvas, gfx::RectFToSkRect(ring_bounds), background_color_,
          progress_color_, kProgressRingStrokeWidth, /*start_angle=*/-90,
          /*sweep_angle=*/360 * download_progress_percentage_ / 100.0);
    }
  }

  ActionItemProgressRingStatus status_ = ActionItemProgressRingStatus::kIdle;
  int download_progress_percentage_ = 0;
  SkColor background_color_ = SK_ColorBLACK;
  SkColor progress_color_ = SK_ColorBLACK;
  gfx::SlideAnimation scanning_animation_{this};
};
BEGIN_METADATA(DownloadProgressRing)
END_METADATA

// Helper class to draw a circular badge with text.
class CircleBadgeImageSource : public gfx::CanvasImageSource {
 public:
  CircleBadgeImageSource(const gfx::Size& size,
                         SkColor background_color,
                         GetBadgeTextCallback get_text_callback)
      : gfx::CanvasImageSource(size),
        background_color_(background_color),
        get_text_callback_(std::move(get_text_callback)) {}

  CircleBadgeImageSource(const CircleBadgeImageSource&) = delete;
  CircleBadgeImageSource& operator=(const CircleBadgeImageSource&) = delete;

  ~CircleBadgeImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(background_color_);

    gfx::RenderText& render_text = get_text_callback_.Run();
    const gfx::Rect& badge_rect = render_text.display_rect();
    // Set the corner radius to make the rectangle appear like a circle.
    const int corner_radius = badge_rect.height() / 2;
    canvas->DrawRoundRect(badge_rect, corner_radius, flags);
    render_text.Draw(canvas);
  }

 private:
  const SkColor background_color_;
  GetBadgeTextCallback get_text_callback_;
};

class DownloadsImageBadge : public views::ImageView {
  METADATA_HEADER(DownloadsImageBadge, views::ImageView)
 public:
  DownloadsImageBadge(DownloadsImageBadge&) = delete;
  DownloadsImageBadge& operator=(const DownloadsImageBadge&) = delete;
  ~DownloadsImageBadge() override = default;

  // Returns the image badge if it is a direct child of `parent`, otherwise
  // creates one and adds it to `parent`. The returned badge is owned by
  // `parent`.
  static DownloadsImageBadge& GetOrInstall(views::View& parent) {
    for (auto& child : parent.children()) {
      if (views::IsViewClass<DownloadsImageBadge>(child)) {
        return *views::AsViewClass<DownloadsImageBadge>(child);
      }
    }
    return *parent.AddChildView(
        base::WrapUnique<DownloadsImageBadge>(new DownloadsImageBadge()));
  }

  void UpdateImage(bool is_active,
                   int progress_download_count,
                   SkColor badge_text_color,
                   SkColor badge_background_color) {
    const int badge_size = std::min(bounds().height(), bounds().width());
    // Only display the badge if there are multiple downloads, or this image
    // view is visible. Use 2dp to make sure that the image has size even with
    // scale factor < 1.0. (this can happen on CrOS).
    if (!is_active || progress_download_count < 2 || badge_size < 2) {
      SetImage(ui::ImageModel());
      return;
    }
    // base::Unretained is safe because this owns the ImageView to which the
    // image source is applied.
    SetImage(ui::ImageModel::FromImageSkia(
        gfx::CanvasImageSource::MakeImageSkia<CircleBadgeImageSource>(
            gfx::Size(badge_size, badge_size), badge_background_color,
            base::BindRepeating(&DownloadsImageBadge::GetBadgeText,
                                base::Unretained(this), progress_download_count,
                                badge_text_color))));
  }

 private:
  // Max download count to show in the badge. Any higher number of downloads
  // results in a placeholder ("9+").
  static constexpr int kMaxDownloadCountDisplayed = 9;

  DownloadsImageBadge() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetCanProcessEventsWithinSubtree(false);
  }

  gfx::RenderText& GetBadgeText(int progress_download_count,
                                SkColor badge_text_color) {
    CHECK_GE(progress_download_count, 2);
    const int badge_height = bounds().height();
    bool use_placeholder = progress_download_count > kMaxDownloadCountDisplayed;
    const int index = use_placeholder ? 0 : progress_download_count - 1;
    gfx::RenderText* render_text = render_texts_.at(index).get();
    if (render_text == nullptr) {
      ui::ResourceBundle* bundle = &ui::ResourceBundle::GetSharedInstance();
      gfx::FontList font = bundle->GetFontList(ui::ResourceBundle::BaseFont)
                               .DeriveWithHeightUpperBound(badge_height);
      std::u16string text =
          use_placeholder
              ? base::StrCat(
                    {base::FormatNumber(kMaxDownloadCountDisplayed), u"+"})
              : base::FormatNumber(progress_download_count);

      std::unique_ptr<gfx::RenderText> new_render_text =
          gfx::RenderText::CreateRenderText();
      new_render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      new_render_text->SetCursorEnabled(false);
      new_render_text->SetFontList(std::move(font));
      new_render_text->SetText(std::move(text));
      new_render_text->SetDisplayRect(
          gfx::Rect(gfx::Point(), gfx::Size(badge_height, badge_height)));

      render_text = new_render_text.get();
      render_texts_[index] = std::move(new_render_text);
    }
    render_text->SetColor(badge_text_color);
    return *render_text;
  }

  // View:
  void Layout(PassKey) override {
    LayoutSuperclass<views::ImageView>(this);
    gfx::Size parent_size = parent()->GetPreferredSize();
    const int badge_height =
        std::min(parent_size.width(), parent_size.height()) / 2;
    const int badge_offset_x = parent_size.width() - badge_height;
    const int badge_offset_y = parent_size.height() - badge_height;
    // If the badge height has changed, clear the cache of render_texts_.
    if (badge_height != bounds().height()) {
      render_texts_ = std::array<std::unique_ptr<gfx::RenderText>,
                                 kMaxDownloadCountDisplayed>{};
    }
    SetBoundsRect(
        gfx::Rect(badge_offset_x, badge_offset_y, badge_height, badge_height));
  }

  // RenderTexts used for the number in the badge. Stores the text for "n" at
  // index n - 1, and stores the text for the placeholder ("9+") at index 0.
  // This is done to avoid re-creating the same RenderText on each paint. Text
  // color of each RenderText is reset upon each paint.
  std::array<std::unique_ptr<gfx::RenderText>, kMaxDownloadCountDisplayed>
      render_texts_{};
};
BEGIN_METADATA(DownloadsImageBadge)
END_METADATA

}  // namespace

DownloadButtonViews::DownloadButtonViews(
    PinnedToolbarActionsContainer& container)
    : container_(container) {}

DownloadButtonViews::~DownloadButtonViews() = default;

bool DownloadButtonViews::IsShowing() const {
  return GetButton().GetVisible();
}

SkColor DownloadButtonViews::GetColor(ui::ColorId color_id) const {
  return GetButton().GetColorProvider()->GetColor(color_id);
}

void DownloadButtonViews::AnnounceAccessibleAlert(const std::u16string& text) {
  GetButton().GetViewAccessibility().AnnounceText(text);
}

void DownloadButtonViews::UpdateProgressRing(
    ActionItemProgressRingStatus status,
    int progress_percentage) {
  DownloadProgressRing& progress_ring =
      DownloadProgressRing::GetOrInstall(GetButton());
  if (status == ActionItemProgressRingStatus::kIdle) {
    progress_ring.SetIdle();
    return;
  }

  // A dormant ring, or the ring of a disabled button, is drawn entirely in the
  // inactive color. Otherwise the ring is drawn in the active color when the
  // button is active, which is indicated by the underline beneath the icon.
  actions::ActionItem* action_item =
      container_->GetActionItemFor(kActionShowDownloads);
  CHECK(action_item);
  const bool is_disabled = status == ActionItemProgressRingStatus::kDormant ||
                           !action_item->GetEnabled();
  const bool is_active =
      action_item->GetProperty(kActionItemUnderlineIndicatorKey);
  const SkColor disabled_color = GetColor(kColorToolbarButtonIconInactive);
  const SkColor background_color =
      is_disabled ? disabled_color
                  : GetColor(kColorDownloadToolbarButtonRingBackground);
  const SkColor progress_color =
      is_disabled ? disabled_color
                  : GetColor(is_active ? kColorDownloadToolbarButtonActive
                                       : kColorDownloadToolbarButtonInactive);
  progress_ring.UpdateColors(background_color, progress_color);

  switch (status) {
    case ActionItemProgressRingStatus::kIdle:
      NOTREACHED();
    case ActionItemProgressRingStatus::kDormant:
      progress_ring.SetDormant();
      break;
    case ActionItemProgressRingStatus::kScanning:
      progress_ring.SetScanning();
      break;
    case ActionItemProgressRingStatus::kDownloading:
      progress_ring.SetDownloading(progress_percentage);
      break;
  }
}

void DownloadButtonViews::UpdateBadge(bool is_active,
                                      int progress_download_count,
                                      SkColor text_color,
                                      SkColor background_color) {
  DownloadsImageBadge::GetOrInstall(GetButton())
      .UpdateImage(is_active, progress_download_count, text_color,
                   background_color);
}

gfx::Rect DownloadButtonViews::GetBoundsInScreen() const {
  return GetButton().GetBoundsInScreen();
}

void DownloadButtonViews::SetElementIdentifier(
    ui::ElementIdentifier element_id) {
  GetButton().SetProperty(views::kElementIdentifierKey, element_id);
}

ActionItemProgressRingStatus
DownloadButtonViews::GetProgressRingStatusForTesting() {
  return DownloadProgressRing::GetOrInstall(GetButton()).GetStatus();
}

views::ImageView* DownloadButtonViews::GetImageBadgeForTesting() {
  return &DownloadsImageBadge::GetOrInstall(GetButton());
}

PinnedActionToolbarButton& DownloadButtonViews::GetButton() const {
  PinnedActionToolbarButton* button =
      container_->GetButtonFor(kActionShowDownloads);
  CHECK(button);
  return *button;
}
