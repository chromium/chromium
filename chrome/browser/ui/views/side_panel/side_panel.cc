// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_resize_area.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"

namespace {

// This thickness includes the solid-color background and the inner round-rect
// border-color stroke. It does not include the outer-color separator.
int GetBorderThickness() {
  return (lens::features::IsLensOverlayEnabled() ? 8 : 16) +
         views::Separator::kThickness;
}

// This is how many units of the toolbar are essentially expected to be
// background.
constexpr int kOverlapFromToolbar = 4;

// We want the border to visually look like GetBorderThickness() units on all
// sides. On the top side, background is drawn on top of the top-content
// separator and some units of background inside the toolbar (or bookmarks bar)
// itself. Subtract both of those to not get visually-excessive padding.
gfx::Insets GetBorderInsets() {
  int border_thickness = GetBorderThickness();
  return gfx::Insets::TLBR(
      border_thickness - views::Separator::kThickness - kOverlapFromToolbar,
      border_thickness, border_thickness, border_thickness);
}

constexpr int kAnimationDurationMs = 450;

// This border paints the toolbar color around the side panel content and draws
// a roundrect viewport around the side panel content. The border can have
// rounded corners of its own.
class SidePanelBorder : public views::Border {
 public:
  explicit SidePanelBorder(BrowserView* browser_view)
      : Border(gfx::kPlaceholderColor), browser_view_(browser_view) {}

  SidePanelBorder(const SidePanelBorder&) = delete;
  SidePanelBorder& operator=(const SidePanelBorder&) = delete;

  void SetHeaderHeight(int height) { header_height_ = height; }
  void SetBorderRadii(const gfx::RoundedCornersF& radii) {
    border_radii_ = radii;
  }

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    // Undo DSF so that we can be sure to draw an integral number of pixels for
    // the border. Integral scale factors should be unaffected by this, but for
    // fractional scale factors this ensures sharp lines.
    gfx::ScopedCanvas scoped_unscale(canvas);
    float dsf = canvas->UndoDeviceScaleFactor();

    const gfx::RectF scaled_view_bounds_f = gfx::ConvertRectToPixels(
        view.GetLocalBounds(), view.layer()->device_scale_factor());

    gfx::RectF scaled_contents_bounds_f = scaled_view_bounds_f;
    const float corner_radius =
        dsf * view.GetLayoutProvider()->GetCornerRadiusMetric(
                  views::ShapeContextTokens::kSidePanelContentRadius);
    const gfx::InsetsF insets_in_pixels(
        gfx::ConvertInsetsToPixels(GetInsets(), dsf));
    scaled_contents_bounds_f.Inset(insets_in_pixels);

    // Use ToEnclosedRect to make sure that the clip bounds never end up larger
    // than the child view.
    gfx::Rect clip_bounds = ToEnclosedRect(scaled_contents_bounds_f);
    SkRRect rect = SkRRect::MakeRectXY(gfx::RectToSkRect(clip_bounds),
                                       corner_radius, corner_radius);

    // Clip out the content area from the background about to be painted.
    canvas->sk_canvas()->clipRRect(rect, SkClipOp::kDifference,
                                   /*do_anti_alias=*/true);

    {
      // Redo the device scale factor. The theme background and clip for the
      // outer corners are drawn in DIPs. Note that the clip area above is in
      // pixels because `UndoDeviceScaleFactor()` was called before this.
      gfx::ScopedCanvas scoped_rescale(canvas);
      canvas->Scale(dsf, dsf);

      const SkScalar border_radii[8] = {
          border_radii_.upper_left(),  border_radii_.upper_left(),
          border_radii_.upper_right(), border_radii_.upper_right(),
          border_radii_.lower_right(), border_radii_.lower_right(),
          border_radii_.lower_left(),  border_radii_.lower_left()};

      SkPath rounded_border_path;
      rounded_border_path.addRoundRect(gfx::RectToSkRect(view.GetLocalBounds()),
                                       border_radii, SkPathDirection::kCW);

      // Add another clip to the canvas that rounds the outer corners of the
      // border. This is done in DIPs because for some device scale factors, the
      // conversion to pixels can cause the clip to be off by a pixel, resulting
      // in a pixel gap between the side panel border and web contents.
      canvas->ClipPath(rounded_border_path, /*do_anti_alias=*/true);

      // Draw the top-container background.
      TopContainerBackground::PaintBackground(canvas, &view, browser_view_);
    }

    // Paint the inner border around SidePanel content. Since half the stroke
    // gets painted in the clipped area, make this twice as thick.
    const float stroke_thickness = views::Separator::kThickness * 2;

    cc::PaintFlags flags;
    flags.setStrokeWidth(stroke_thickness);
    flags.setColor(
        view.GetColorProvider()->GetColor(kColorSidePanelContentAreaSeparator));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setAntiAlias(true);

    canvas->sk_canvas()->drawRRect(rect, flags);
  }

  gfx::Insets GetInsets() const override {
    // This additional inset matches the growth inside BorderView::Layout()
    // below to let us paint on top of the toolbar separator. This additional
    // inset is outside the SidePanel itself, but not outside the BorderView. If
    // there is a header we want to increase the top inset to give room for the
    // header to paint on top of the border area.
    int top_inset =
        views::Separator::kThickness + header_height_ - GetBorderThickness();
    return GetBorderInsets() + gfx::Insets::TLBR(top_inset, 0, 0, 0);
  }
  gfx::Size GetMinimumSize() const override {
    return gfx::Size(GetInsets().width(), GetInsets().height());
  }

 private:
  int header_height_ = 0;
  gfx::RoundedCornersF border_radii_;
  const raw_ptr<BrowserView> browser_view_;
};

class BorderView : public views::View {
  METADATA_HEADER(BorderView, views::View)

 public:
  explicit BorderView(BrowserView* browser_view) {
    SetVisible(false);
    auto border = std::make_unique<SidePanelBorder>(browser_view);
    border_ = border.get();
    SetBorder(std::move(border));
    // Don't allow the view to process events. If we do allow this then events
    // won't get passed on to the side panel hosted content.
    SetCanProcessEventsWithinSubtree(false);
  }

  void HeaderViewChanged(views::View* header_view) {
    border_->SetHeaderHeight(
        header_view ? header_view->GetPreferredSize().height() : 0);
  }

  void SetBorderRadii(const gfx::RoundedCornersF& radii) {
    border_->SetBorderRadii(radii);
    SchedulePaint();
  }

  void Layout(PassKey) override {
    // Let BorderView grow slightly taller so that it overlaps the divider into
    // the toolbar or bookmarks bar above it.
    gfx::Rect bounds = parent()->GetLocalBounds();
    bounds.Inset(gfx::Insets::TLBR(-views::Separator::kThickness, 0, 0, 0));

    SetBoundsRect(bounds);
  }

  void OnThemeChanged() override {
    SchedulePaint();
    View::OnThemeChanged();
  }

 private:
  raw_ptr<SidePanelBorder> border_;
};

BEGIN_METADATA(BorderView)
END_METADATA

// ContentParentView is the parent view for views hosted in the
// side panel.
class ContentParentView : public views::View {
  METADATA_HEADER(ContentParentView, views::View)

 public:
  ContentParentView() {
    SetUseDefaultFillLayout(true);
    SetBackground(
        views::CreateThemedSolidBackground(kColorSidePanelBackground));
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
  }

  ~ContentParentView() override = default;
};

BEGIN_METADATA(ContentParentView)
END_METADATA

}  // namespace

// Ensures immediate children of the SidePanel have their layers clipped to
// their visible bounds to prevent incorrect clipping during animation.
// TODO: 344626785 - Remove this once WebView layer behavior has been fixed.
class SidePanel::VisibleBoundsViewClipper : public views::ViewObserver {
 public:
  explicit VisibleBoundsViewClipper(SidePanel* side_panel)
      : side_panel_(side_panel) {
    view_observations_.AddObservation(side_panel);
  }
  VisibleBoundsViewClipper(const VisibleBoundsViewClipper&) = delete;
  VisibleBoundsViewClipper& operator=(const VisibleBoundsViewClipper&) = delete;
  ~VisibleBoundsViewClipper() override = default;

  // views::ViewObserver:
  void OnChildViewAdded(View* observed_view, View* child) override {
    if (observed_view == side_panel_) {
      view_observations_.AddObservation(child);
    }
  }
  void OnViewBoundsChanged(views::View* observed_view) override {
    ui::Layer* layer = observed_view->layer();
    if (observed_view != side_panel_ && layer) {
      gfx::Rect clip_bounds = observed_view->GetVisibleBounds();
      // Let side panel grow slightly taller so that it overlaps the divider
      // into the toolbar or bookmarks bar above it.
      // TODO: Explore extending the side panel bounds directly in
      // BrowserViewLayout.
      clip_bounds.Inset(
          gfx::Insets::TLBR(-views::Separator::kThickness, 0, 0, 0));
      // Only clip the bounds while animating. This makes sure we don't
      // incorrectly clip things like focus rings for header buttons or the
      // resize handle.
      layer->SetClipRect(side_panel_->GetAnimationValue() < 1 ? clip_bounds
                                                              : gfx::Rect());
      layer->SetVisible(clip_bounds.width() != 0);
    }
  }
  void OnViewIsDeleting(views::View* observed_view) override {
    view_observations_.RemoveObservation(observed_view);
  }

 private:
  // Owns this.
  const raw_ptr<SidePanel> side_panel_;

  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      view_observations_{this};
};

SidePanel::SidePanel(BrowserView* browser_view,
                     HorizontalAlignment horizontal_alignment)
    : views::AnimationDelegateViews(this),
      browser_view_(browser_view),
      horizontal_alignment_(horizontal_alignment) {
  if (lens::features::IsLensOverlayEnabled()) {
    visible_bounds_view_clipper_ =
        std::make_unique<VisibleBoundsViewClipper>(this);
  }
  std::unique_ptr<BorderView> border_view =
      std::make_unique<BorderView>(browser_view);
  border_view_ = border_view.get();
  AddChildView(std::move(border_view));

  std::unique_ptr<views::SidePanelResizeArea> resize_area =
      std::make_unique<views::SidePanelResizeArea>(this);
  resize_area_ = resize_area.get();
  AddChildView(std::move(resize_area));

  pref_change_registrar_.Init(browser_view->GetProfile()->GetPrefs());

  // base::Unretained is safe since the side panel must be attached to some
  // BrowserView. Deleting BrowserView will also delete the SidePanel.
  pref_change_registrar_.Add(
      prefs::kSidePanelHorizontalAlignment,
      base::BindRepeating(&BrowserView::UpdateSidePanelHorizontalAlignment,
                          base::Unretained(browser_view)));

  animation_.SetTweenType(gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED);

  animation_.SetSlideDuration(base::Milliseconds(kAnimationDurationMs));

  SetVisible(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Set the panel width from the preference or use the minimum size as the
  // default.
  SetPanelWidth(GetMinimumSize().width());

  SetBorder(views::CreateEmptyBorder(GetBorderInsets()));

  SetProperty(views::kElementIdentifierKey, kSidePanelElementId);

  content_parent_view_ = AddChildView(std::make_unique<ContentParentView>());
  content_parent_view_->SetVisible(false);
}

SidePanel::~SidePanel() = default;

void SidePanel::SetPanelWidth(int width) {
  // Only the width is used by BrowserViewLayout.
  SetPreferredSize(gfx::Size(width, 1));
}

void SidePanel::SetBackgroundRadii(const gfx::RoundedCornersF& radii) {
  if (radii == background_radii_) {
    return;
  }
  background_radii_ = radii;

  // Since the border_view paints the background, by adding rounded
  // corners to border will paint a rounded background for the side panel.
  static_cast<BorderView*>(border_view_)->SetBorderRadii(background_radii_);
}

void SidePanel::UpdateWidthOnEntryChanged() {
  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  ScopedDictPrefUpdate update(pref_service, prefs::kSidePanelIdToWidth);
  const base::Value::Dict& dict = update.Get();
  SidePanelUI* coordinator =
      browser_view_->browser()->GetFeatures().side_panel_ui();
  if (coordinator) {
    std::optional<SidePanelEntry::Id> entry_id =
        coordinator->GetCurrentEntryId();
    if (entry_id.has_value()) {
      std::string panel_id = SidePanelEntryIdToString(entry_id.value());
      std::optional<int> width = dict.FindInt(panel_id);
      if (width.has_value()) {
        SetPanelWidth(width.value());
      } else {
        SetPanelWidth(GetMinimumSize().width());
      }
    }
  }
}

void SidePanel::SetHorizontalAlignment(HorizontalAlignment alignment) {
  horizontal_alignment_ = alignment;
}

SidePanel::HorizontalAlignment SidePanel::GetHorizontalAlignment() {
  return horizontal_alignment_;
}

bool SidePanel::IsRightAligned() {
  return GetHorizontalAlignment() == HorizontalAlignment::kRight;
}

gfx::Size SidePanel::GetMinimumSize() const {
  const int min_side_panel_contents_width = 360;
  const int min_height = 0;
  return gfx::Size(min_side_panel_contents_width + GetBorderInsets().width(),
                   min_height);
}

bool SidePanel::IsClosing() {
  return animation_.IsClosing();
}

void SidePanel::AddHeaderView(std::unique_ptr<views::View> view) {
  // If a header view already exists make sure we remove it so that it is
  // replaced.
  if (header_view_) {
    RemoveChildView(header_view_);
  }
  header_view_ = view.get();
  AddChildView(std::move(view));
  static_cast<BorderView*>(border_view_)->HeaderViewChanged(header_view_);
  // Update the border so that the insets include space for the header to be
  // placed on top of the border.
  int top_inset = header_view_->height() - GetBorderThickness();
  SetBorder(views::CreateEmptyBorder(GetBorderInsets() +
                                     gfx::Insets::TLBR(top_inset, 0, 0, 0)));
}

gfx::Size SidePanel::GetContentSizeUpperBound() const {
  const int side_panel_width = width() > 0 ? width() : GetMinimumSize().width();
  const int side_panel_height =
      height() > 0 ? height() : browser_view_->height();

  return gfx::Size(std::max(0, side_panel_width - GetBorderInsets().width()),
                   std::max(0, side_panel_height - GetBorderInsets().height()));
}

void SidePanel::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (previous_bounds.width() != width() && keyboard_resized_) {
    keyboard_resized_ = false;
    AnnounceResize();
  }
}

double SidePanel::GetAnimationValue() const {
  if (ShouldShowAnimation()) {
    return animation_.GetCurrentValue();
  } else {
    return 1;
  }
}

void SidePanel::OnChildViewAdded(View* observed_view, View* child) {
  if (observed_view != this || child == border_view_ || child == resize_area_) {
    return;
  }
  if (child != header_view_) {
    content_view_observations_.AddObservation(child);
  }

  // Reorder `border_view_` to be last so that it gets painted on top, even if
  // an added child also paints to a layer.
  ReorderChildView(border_view_, children().size());

  // Reorder `header_view_` if it exists to get painted on top of the border
  // view.
  if (header_view_) {
    ReorderChildView(header_view_, children().size());
  }
  // Reorder `resize_area_` to be last so that it gets painted on top of
  // `border_view_`, for displaying the resize handle.
  ReorderChildView(resize_area_, children().size());

  if (header_view_) {
    // The header view should come before all other side panel children except
    // the resize area in focus order.
    header_view_->InsertBeforeInFocusList(GetChildrenFocusList().front());
  }
  // The resize area should come before all other side panel children in focus
  // order.
  resize_area_->InsertBeforeInFocusList(GetChildrenFocusList().front());
}

void SidePanel::OnChildViewRemoved(View* observed_view, View* child) {
  if (observed_view != this) {
    return;
  }
  if (content_view_observations_.IsObservingSource(child)) {
    content_view_observations_.RemoveObservation(child);
  }
}

void SidePanel::AnimationProgressed(const gfx::Animation* animation) {
  base::TimeDelta step_time =
      base::TimeTicks::Now() - last_animation_step_timestamp_;
  last_animation_step_timestamp_ = base::TimeTicks::Now();
  if (!largest_animation_step_time_.has_value() ||
      largest_animation_step_time_ < step_time) {
    largest_animation_step_time_ = step_time;
  }
  InvalidateLayout();
}

void SidePanel::AnimationEnded(const gfx::Animation* animation) {
  if (animation->GetCurrentValue() == 0) {
    SetVisible(false);
    state_ = State::kClosed;
  } else {
    state_ = State::kOpen;
  }
  if (largest_animation_step_time_.has_value()) {
    SidePanelUtil::RecordSidePanelAnimationMetrics(
        largest_animation_step_time_.value());
  }
  InvalidateLayout();
}

void SidePanel::UpdateSidePanelWidthPref(const std::string& panel_id,
                                         int width) {
  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  ScopedDictPrefUpdate update(pref_service, prefs::kSidePanelIdToWidth);
  base::Value::Dict& dict = update.Get();

  // Update the dictionary with the new width for the specified panel_id.
  dict.Set(panel_id, base::Value(width));
}

void SidePanel::OnResize(int resize_amount, bool done_resizing) {
  if (starting_width_on_resize_ < 0) {
    starting_width_on_resize_ = width();
  }

  int proposed_width = starting_width_on_resize_ +
                       ((IsRightAligned() && !base::i18n::IsRTL()) ||
                                (!IsRightAligned() && base::i18n::IsRTL())
                            ? -resize_amount
                            : resize_amount);
  if (done_resizing) {
    starting_width_on_resize_ = -1;
  }

  const int minimum_width = GetMinimumSize().width();
  if (proposed_width < minimum_width) {
    proposed_width = minimum_width;
  }

  if (width() != proposed_width) {
    if (base::FeatureList::IsEnabled(features::kSidePanelResizing)) {
      auto* coordinator =
          browser_view_->browser()->GetFeatures().side_panel_ui();
      if (coordinator) {
        std::optional<SidePanelEntry::Id> entry_id =
            coordinator->GetCurrentEntryId();
        if (entry_id.has_value()) {
          std::string panel_id = SidePanelEntryIdToString(entry_id.value());

          // Update the pref with the new width
          UpdateSidePanelWidthPref(panel_id, proposed_width);
        }
      }
    }

    SetPanelWidth(proposed_width);
    did_resize_ = true;
  }
}

void SidePanel::RecordMetricsIfResized() {
  if (did_resize_) {
    std::optional<SidePanelEntry::Id> id = browser_view_->browser()
                                               ->GetFeatures()
                                               .side_panel_ui()
                                               ->GetCurrentEntryId();
    if (!id.has_value()) {
      return;
    }
    int side_panel_contents_width = width() - GetBorderInsets().width();
    int browser_window_width = browser_view_->width();
    SidePanelUtil::RecordSidePanelResizeMetrics(
        id.value(), side_panel_contents_width, browser_window_width);
    did_resize_ = false;
  }
}

void SidePanel::Open(bool animated) {
  UpdateVisibility(/*should_be_open=*/true, animated);
}

void SidePanel::Close(bool animated) {
  UpdateVisibility(/*should_be_open=*/false, animated);
}

views::View* SidePanel::GetContentParentView() {
  return content_parent_view_;
}

void SidePanel::UpdateVisibility(bool should_be_open, bool animate_transition) {
  if (should_be_open) {
    state_ = animate_transition ? State::kOpening : State::kOpen;
  } else {
    state_ = animate_transition ? State::kClosing : State::kClosed;
  }
  std::vector<views::View*> views_to_hide;
  // TODO(pbos): Iterate content instead. Requires moving the owned pointer out
  // of owned contents before resetting it.
  for (views::View* view : children()) {
    if (view == border_view_ || view == resize_area_ || view == header_view_ ||
        !view->GetVisible()) {
      continue;
    }

    if (state_ == State::kClosing || state_ == State::kClosed) {
      views_to_hide.push_back(view);
    }
  }
  // Make sure the border visibility matches the side panel. Also dynamically
  // create and destroy the layer to reclaim memory and avoid painting and
  // compositing this border when it's not showing. See
  // https://crbug.com/1269090.
  // TODO(pbos): Should layer visibility/painting be automatically tied to
  // parent visibility? I.e. the difference between GetVisible() and IsDrawn().
  bool side_panel_open_or_closing = GetVisible() || should_be_open;
  if (side_panel_open_or_closing != border_view_->GetVisible()) {
    border_view_->SetVisible(side_panel_open_or_closing);
    if (side_panel_open_or_closing) {
      border_view_->SetPaintToLayer();
      border_view_->layer()->SetFillsBoundsOpaquely(false);
      if (header_view_) {
        static_cast<BorderView*>(border_view_)->HeaderViewChanged(header_view_);
        int top_inset = header_view_->height() - GetBorderThickness();
        SetBorder(views::CreateEmptyBorder(
            GetBorderInsets() + gfx::Insets::TLBR(top_inset, 0, 0, 0)));
      }
    } else {
      border_view_->DestroyLayer();
    }
  }
  if (ShouldShowAnimation() && animate_transition) {
    if (should_be_open) {
      // If the side panel should remain open but there are views to hide, hide
      // them immediately.
      for (auto* view : views_to_hide) {
        view->SetVisible(false);
      }
      SetVisible(should_be_open);
      largest_animation_step_time_.reset();
      last_animation_step_timestamp_ = base::TimeTicks::Now();
      animation_.Show();
    } else if (GetVisible() && !IsClosing()) {
      largest_animation_step_time_.reset();
      last_animation_step_timestamp_ = base::TimeTicks::Now();
      animation_.Hide();
    }
  } else {
    // Set the animation value so that it accurately reflects what state the
    // side panel should be in for layout.
    animation_.Reset(should_be_open ? 1 : 0);
    SetVisible(should_be_open);
  }
}

bool SidePanel::ShouldShowAnimation() const {
  return lens::features::IsLensOverlayEnabled() &&
         gfx::Animation::ShouldRenderRichAnimation() && !animations_disabled_;
}

void SidePanel::AnnounceResize() {
  float side_panel_width = width();
  float web_contents_width =
      browser_view_->contents_container()->bounds().width();
  float total_width = browser_view_->bounds().width();
  int side_panel_percentage = (side_panel_width / total_width) * 100;
  int web_contents_percentage = (web_contents_width / total_width) * 100;
  if (side_panel_percentage + web_contents_percentage > 100) {
    side_panel_percentage--;
  }
  bool side_panel_right_aligned = IsRightAligned();
  std::u16string web_contents_side_text = l10n_util::GetStringUTF16(
      side_panel_right_aligned
          ? IDS_SIDE_PANEL_RESIZE_LEFT_SIDE_ACCESSIBLE_ALERT
          : IDS_SIDE_PANEL_RESIZE_RIGHT_SIDE_ACCESSIBLE_ALERT);
  std::u16string side_panel_side_text = l10n_util::GetStringUTF16(
      side_panel_right_aligned
          ? IDS_SIDE_PANEL_RESIZE_RIGHT_SIDE_ACCESSIBLE_ALERT
          : IDS_SIDE_PANEL_RESIZE_LEFT_SIDE_ACCESSIBLE_ALERT);

  GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
      IDS_SIDE_PANEL_RESIZE_ACCESSIBLE_ALERT, web_contents_side_text,
      base::FormatPercent(web_contents_percentage), side_panel_side_text,
      base::FormatPercent(side_panel_percentage)));
}

BEGIN_METADATA(SidePanel)
END_METADATA
