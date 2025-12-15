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
#include "chrome/browser/ui/views/side_panel/side_panel_animation_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_ids.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_resize_area.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
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
  return 8 + views::Separator::kThickness;
}

// This is how many units of the toolbar are essentially expected to be
// background.
constexpr int kOverlapFromToolbar = 4;

// We want the border to visually look like GetBorderThickness() units on all
// sides except the top. On the top side, background is drawn on top of the
// top-content separator and some units of background inside the toolbar (or
// bookmarks bar) itself. Subtract both of those to not get visually-excessive
// padding.
gfx::Insets GetBorderInsets() {
  int border_thickness = GetBorderThickness();
  return gfx::Insets::TLBR(-kOverlapFromToolbar, border_thickness,
                           border_thickness, border_thickness);
}

SidePanel::HorizontalAlignment GetHorizontalAlignment(
    PrefService* pref_service,
    SidePanelEntry::PanelType type) {
  bool is_right_aligned =
      pref_service->GetBoolean(prefs::kSidePanelHorizontalAlignment);
  is_right_aligned = type == SidePanelEntry::PanelType::kToolbar &&
                             features::kSidePanelRelativeAlignment.Get() ==
                                 features::SidePanelRelativeAlignment::
                                     kShowPanelsOnOppositeSides
                         ? !is_right_aligned
                         : is_right_aligned;
  return is_right_aligned ? SidePanel::HorizontalAlignment::kRight
                          : SidePanel::HorizontalAlignment::kLeft;
}

// This border paints the toolbar color around the side panel content and draws
// a roundrect viewport around the side panel content. The border can have
// rounded corners of its own.
class SidePanelBorder : public views::Border {
 public:
  explicit SidePanelBorder(BrowserView* browser_view)
      : browser_view_(browser_view) {
    SetColor(kColorSidePanelContentAreaSeparator);
  }

  SidePanelBorder(const SidePanelBorder&) = delete;
  SidePanelBorder& operator=(const SidePanelBorder&) = delete;

  void SetHeaderHeight(int height) { header_height_ = height; }
  void SetBorderRadii(const gfx::RoundedCornersF& radii) {
    border_radii_ = radii;
  }

  void SetOutlineVisibility(bool visible) { outline_visible_ = visible; }

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

      const SkVector border_radii[4] = {
          {border_radii_.upper_left(), border_radii_.upper_left()},
          {border_radii_.upper_right(), border_radii_.upper_right()},
          {border_radii_.lower_right(), border_radii_.lower_right()},
          {border_radii_.lower_left(), border_radii_.lower_left()}};

      const SkPath rounded_border_path = SkPath::RRect(SkRRect::MakeRectRadii(
          gfx::RectToSkRect(view.GetLocalBounds()), border_radii));

      // Add another clip to the canvas that rounds the outer corners of the
      // border. This is done in DIPs because for some device scale factors, the
      // conversion to pixels can cause the clip to be off by a pixel, resulting
      // in a pixel gap between the side panel border and web contents.
      canvas->ClipPath(rounded_border_path, /*do_anti_alias=*/true);

      // Draw the top-container background.
      TopContainerBackground::PaintBackground(canvas, &view, browser_view_);
    }

    // Paint the inner border around SidePanel content. Since half the stroke
    // gets painted in the clipped area, make this twice as thick, and scale
    // the thickness by device scale factor since we're working in pixels.
    const float stroke_thickness =
        outline_visible_
            ? views::Separator::kThickness * 2 * dsf
            // TODO(crbug.com/463994274): Avoid drawing a hairline stroke.
            : 0;

    cc::PaintFlags flags;
    flags.setStrokeWidth(stroke_thickness);
    flags.setColor(color().ResolveToSkColor(view.GetColorProvider()));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setAntiAlias(true);
    if (!outline_visible_) {
      // TODO(crbug.com/463994274): Zero stroke width still draws a hairline. We
      // can't remove this rectangle, or we get some visual artifacts, so
      // instead just draw it in the background color.
      std::optional<SkColor> bg_color =
          TopContainerBackground::GetBackgroundColor(&view, browser_view_);
      if (bg_color) {
        flags.setColor(*bg_color);
      }
    }
    canvas->sk_canvas()->drawRRect(rect, flags);
  }

  gfx::Insets GetInsets() const override {
    // This additional inset matches the growth inside BorderView::Layout()
    // below to let us paint on top of the toolbar separator. This additional
    // inset is outside the SidePanel itself, but not outside the BorderView. If
    // there is a header we want to increase the top inset to give room for the
    // header to paint on top of the border area.
    int top_inset =
        views::Separator::kThickness + header_height_ - GetBorderInsets().top();
    return GetBorderInsets() + gfx::Insets::TLBR(top_inset, 0, 0, 0);
  }
  gfx::Size GetMinimumSize() const override {
    return gfx::Size(GetInsets().width(), GetInsets().height());
  }

 private:
  int header_height_ = 0;
  gfx::RoundedCornersF border_radii_;
  bool outline_visible_ = true;
  const raw_ptr<BrowserView> browser_view_;
};

// ContentParentView is the parent view for views hosted in the
// side panel.
class ContentParentView : public views::View, public views::ViewObserver {
  METADATA_HEADER(ContentParentView, views::View)

 public:
  explicit ContentParentView(bool should_round_corners,
                             SidePanelEntry::PanelType type)
      : should_round_corners_(should_round_corners), type_(type) {
    SetUseDefaultFillLayout(true);
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    // If corners should be rounded, observe this view to round corners of
    // children as they are added.
    if (should_round_corners_) {
      view_observation_.Observe(this);
    }
  }

  ~ContentParentView() override = default;

 private:
  void AddedToWidget() override {
    SetBackground(views::CreateRoundedRectBackground(kColorSidePanelBackground,
                                                     GetRoundedCorners()));
  }

  void OnChildViewAdded(views::View* observed_view,
                        views::View* child) override {
    // We must use ViewObserver::OnChildViewAdded instead of
    // View::ViewHierarchyChanged here because setting rounded corners on a
    // WebView's holder requires the NativeViewHost's native_wrapper_ to be set
    // and this gets set in View::ViewHierarchyChanged which OnChildViewAdded
    // will be called after View::ViewHierarchyChanged.
    // If the child is a WebView or paints to a layer, round its corners.
    if (views::IsViewClass<views::WebView>(child)) {
      views::AsViewClass<views::WebView>(child)->holder()->SetCornerRadii(
          GetRoundedCorners());
    }
    if (child->layer()) {
      child->layer()->SetRoundedCornerRadius(GetRoundedCorners());
      child->layer()->SetIsFastRoundedCorner(true);
    }
  }

  gfx::RoundedCornersF GetRoundedCorners() {
    return should_round_corners_ && GetLayoutProvider()
               ? gfx::RoundedCornersF(
                     GetLayoutProvider()->GetCornerRadiusMetric(
                         type_ == SidePanelEntry::PanelType::kToolbar
                             ? views::ShapeContextTokens::
                                   kToolbarHeightSidePanelContentRadius
                             : views::ShapeContextTokens::
                                   kSidePanelContentRadius))
               : gfx::RoundedCornersF();
  }

  bool should_round_corners_ = false;
  SidePanelEntry::PanelType type_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
};

BEGIN_METADATA(ContentParentView)
END_METADATA

}  // namespace

class SidePanel::BorderView : public views::View {
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

  void SetOutlineVisibilty(bool visible) {
    border_->SetOutlineVisibility(visible);
    SchedulePaint();
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

BEGIN_METADATA(SidePanel, BorderView)
END_METADATA

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
                     SidePanelEntry::PanelType type,
                     bool has_border)
    : browser_view_(browser_view),
      type_(type),
      visible_bounds_view_clipper_(
          std::make_unique<VisibleBoundsViewClipper>(this)) {
  horizontal_alignment_ =
      GetHorizontalAlignment(browser_view->GetProfile()->GetPrefs(), type_);

  // The default z-order is the order in which children were added to the
  // parent view. content_parent_view_ is added first so it exists behind
  // border_view_ and resize_area_.
  content_parent_view_ = AddChildView(std::make_unique<ContentParentView>(
      /*should_round_corners=*/!has_border, type));
  content_parent_view_->SetVisible(false);

  if (has_border) {
    std::unique_ptr<BorderView> border_view =
        std::make_unique<BorderView>(browser_view);
    border_view_ = border_view.get();
    AddChildView(std::move(border_view));
  }

  std::unique_ptr<views::SidePanelResizeArea> resize_area =
      std::make_unique<views::SidePanelResizeArea>(this);
  resize_area_ = resize_area.get();
  AddChildView(std::move(resize_area));
  resize_area_->InsertBeforeInFocusList(content_parent_view_);

  pref_change_registrar_.Init(browser_view->GetProfile()->GetPrefs());

  pref_change_registrar_.Add(
      prefs::kSidePanelHorizontalAlignment,
      base::BindRepeating(&SidePanel::UpdateHorizontalAlignment,
                          base::Unretained(this)));

  animation_coordinator_ =
      std::make_unique<SidePanelAnimationCoordinator>(this);
  animation_coordinator_->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kOpen, this);
  animation_coordinator_->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kClose, this);
  animation_coordinator_->AddObserver(
      SidePanelAnimationCoordinator::AnimationType::kOpenWithContentTransition,
      this);

  animation_coordinator_->AddObserver(kSidePanelBoundsAnimation, this);

  animation_coordinator_->AddObserver(kSidePanelContentOpacityAnimation, this);
  animation_coordinator_->AddObserver(kSidePanelContentCornerRadiusAnimation,
                                      this);

  SetVisible(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Set the panel width from the preference or use the minimum size as the
  // default.
  SetPanelWidth(GetMinimumSize().width());

  if (has_border) {
    SetBorder(views::CreateEmptyBorder(GetBorderInsets()));
  }

  SetProperty(views::kElementIdentifierKey, kSidePanelElementId);
}

SidePanel::~SidePanel() {
  animation_coordinator_->RemoveObserver(
      SidePanelAnimationCoordinator::AnimationType::kOpen, this);
  animation_coordinator_->RemoveObserver(
      SidePanelAnimationCoordinator::AnimationType::kClose, this);
  animation_coordinator_->RemoveObserver(
      SidePanelAnimationCoordinator::AnimationType::kOpenWithContentTransition,
      this);

  animation_coordinator_->RemoveObserver(kSidePanelBoundsAnimation, this);

  animation_coordinator_->RemoveObserver(kSidePanelContentOpacityAnimation,
                                         this);
  animation_coordinator_->RemoveObserver(kSidePanelContentCornerRadiusAnimation,
                                         this);
}

void SidePanel::SetPanelWidth(int width) {
  // Only the width is used by BrowserViewLayout.
  SetPreferredSize(gfx::Size(width, 1));
}

bool SidePanel::ShouldRestrictMaxWidth() const {
  // TODO(crbug.com/394339052): Only restricting width for only non-read
  // anything content is a temporary solution and UX will investigate a better
  // long term solution.
  SidePanelUI* side_panel_ui =
      browser_view_->browser()->GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return true;
  }
  return !side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything));
}

void SidePanel::SetBackgroundRadii(const gfx::RoundedCornersF& radii) {
  if (radii == background_radii_) {
    return;
  }
  background_radii_ = radii;

  if (border_view_) {
    // Since the border_view paints the background, by adding rounded
    // corners to border will paint a rounded background for the side panel.
    border_view_->SetBorderRadii(background_radii_);
  }
}

void SidePanel::UpdateWidthOnEntryChanged() {
  SidePanelUI* side_panel_ui =
      browser_view_->browser()->GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return;
  }

  std::optional<SidePanelEntry::Id> current_entry =
      side_panel_ui->GetCurrentEntryId(type_);
  if (!current_entry) {
    return;
  }

  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  const base::Value::Dict& dict =
      pref_service->GetDict(prefs::kSidePanelIdToWidth);
  std::string panel_id = SidePanelEntryIdToString(current_entry.value());

  // Figure out for this side panel if we should:
  // 1. Use the user's manually resized width
  // 2. Use the side panels default width
  // NOTE: If not specified, the side panel will default to
  // SidePanelEntry::kSidePanelDefaultContentWidth which evaluates to the same
  // value as GetMinimumSize().
  if (std::optional<int> width_from_pref = dict.FindInt(panel_id)) {
    SetPanelWidth(width_from_pref.value());
  } else {
    SetPanelWidth(side_panel_ui->GetCurrentEntryDefaultContentWidth(type_) +
                  GetBorderInsets().width());
  }
}

bool SidePanel::IsRightAligned() const {
  return horizontal_alignment() == HorizontalAlignment::kRight;
}

gfx::Size SidePanel::GetMinimumSize() const {
  const int min_height = 0;
  return gfx::Size(
      SidePanelEntry::kSidePanelDefaultContentWidth + GetBorderInsets().width(),
      min_height);
}

bool SidePanel::IsClosing() {
  return animation_coordinator_->IsClosing();
}

gfx::Rect SidePanel::GetContentAnimationBounds(
    const gfx::Rect& side_panel_final_bounds) {
  CHECK(content_starting_bounds_.has_value());
  gfx::Rect content_starting_bounds = content_starting_bounds_.value();
  // Inset the final bounds to get the content's final bounds.
  gfx::Rect final_bounds = side_panel_final_bounds;
  final_bounds.Inset(GetInsets());

  gfx::Rect animating_bounds;
  double top_bound_animation_value =
      GetAnimationValueFor(kSidePanelContentTopBoundAnimation);
  animating_bounds.set_y(gfx::Tween::IntValueBetween(
      top_bound_animation_value, content_starting_bounds.y(),
      final_bounds.y()));
  double bottom_bound_animation_value =
      GetAnimationValueFor(kSidePanelContentBottomBoundAnimation);
  animating_bounds.set_height(
      gfx::Tween::IntValueBetween(bottom_bound_animation_value,
                                  content_starting_bounds.bottom(),
                                  final_bounds.bottom()) -
      animating_bounds.y());
  double left_bound_animation_value =
      GetAnimationValueFor(kSidePanelContentLeftBoundAnimation);
  animating_bounds.set_x(gfx::Tween::IntValueBetween(
      left_bound_animation_value, content_starting_bounds.x(),
      final_bounds.x()));
  double width_bound_animation_value =
      GetAnimationValueFor(kSidePanelContentWidthBoundAnimation);
  animating_bounds.set_width(gfx::Tween::IntValueBetween(
      width_bound_animation_value, content_starting_bounds.width(),
      final_bounds.width()));
  return animating_bounds;
}

void SidePanel::AddHeaderView(std::unique_ptr<views::View> view) {
  // If a header view already exists make sure we remove it so that it is
  // replaced.
  if (header_view_) {
    auto header_view = RemoveChildViewT(header_view_);
    header_view_ = nullptr;
  }
  header_view_ = view.get();
  AddChildView(std::move(view));
  header_view_->InsertAfterInFocusList(resize_area_);
  header_view_->DeprecatedLayoutImmediately();
  if (border_view_) {
    border_view_->HeaderViewChanged(header_view_);
  }
  // Update the border so that the insets include space for the header to be
  // placed on top of the border area.
  int top_inset = header_view_->height() - GetBorderInsets().top();
  SetBorder(views::CreateEmptyBorder(GetBorderInsets() +
                                     gfx::Insets::TLBR(top_inset, 0, 0, 0)));
}

void SidePanel::RemoveHeaderView() {
  SetBorder(views::CreateEmptyBorder(GetBorderInsets().set_top(0)));
  if (border_view_) {
    border_view_->HeaderViewChanged(nullptr);
  }
  if (header_view_) {
    auto header_view = RemoveChildViewT(header_view_);
    header_view_ = nullptr;
  }
}

void SidePanel::SetOutlineVisibility(bool visible) {
  if (!border_view_) {
    return;
  }
  border_view_->SetOutlineVisibilty(visible);
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
  return GetAnimationValueFor(kSidePanelBoundsAnimation);
}

void SidePanel::OnAnimationSequenceProgressed(
    const SidePanelAnimationCoordinator::SidePanelAnimationId& animation_id,
    double animation_value) {
  if (animation_id == kSidePanelContentCornerRadiusAnimation) {
    CHECK(browser_view_->GetSidePanelAnimationContent());
    CHECK(browser_view_->GetSidePanelAnimationContent()->layer());
    const gfx::RoundedCornersF kRoundedCorners{
        gfx::Tween::FloatValueBetween(animation_value, 0, 16)};
    browser_view_->GetSidePanelAnimationContent()
        ->layer()
        ->SetRoundedCornerRadius(kRoundedCorners);
  } else if (animation_id == kSidePanelContentOpacityAnimation) {
    CHECK(browser_view_->GetSidePanelAnimationContent());
    CHECK(browser_view_->GetSidePanelAnimationContent()->layer());
    browser_view_->GetSidePanelAnimationContent()->layer()->SetOpacity(
        gfx::Tween::DoubleValueBetween(animation_value, 0.5, 1));
  } else if (animation_id == kSidePanelBoundsAnimation) {
    const base::TimeTicks now = base::TimeTicks::Now();
    const base::TimeDelta elapsed = now - last_animation_step_timestamp_;
    last_animation_step_timestamp_ = now;

    if (!largest_animation_step_time_.has_value() ||
        elapsed > largest_animation_step_time_.value()) {
      largest_animation_step_time_ = elapsed;
    }
    InvalidateLayout();
  } else {
    NOTREACHED() << "Observed animation id is not handled";
  }
}

void SidePanel::OnAnimationTypeStarted(
    SidePanelAnimationCoordinator::AnimationType type) {
  if (type == SidePanelAnimationCoordinator::AnimationType::
                  kOpenWithContentTransition) {
    views::View* animation_content =
        browser_view_->GetSidePanelAnimationContent();
    CHECK(animation_content);
    CHECK(animation_content->layer());
    animation_content->layer()->SetOpacity(0.5);
  }
}

void SidePanel::OnAnimationTypeEnded(
    SidePanelAnimationCoordinator::AnimationType type) {
  switch (type) {
    case SidePanelAnimationCoordinator::AnimationType::
        kOpenWithContentTransition:
      if (browser_view_->GetSidePanelAnimationContent()) {
        content_parent_view_->AddChildView(
            browser_view_->GetSidePanelAnimationContent());
        browser_view_->SetSidePanelAnimationContent(nullptr);
      }
      content_starting_bounds_.reset();
      [[fallthrough]];
    case SidePanelAnimationCoordinator::AnimationType::kOpen:
      state_ = State::kOpen;
      break;
    case SidePanelAnimationCoordinator::AnimationType::kClose:
      state_ = State::kClosed;
      SetVisible(false);
      break;
    default:
      NOTREACHED() << "Observed animation type is not handled";
  }

  if (largest_animation_step_time_.has_value()) {
    SidePanelUtil::RecordSidePanelAnimationMetrics(
        type_, largest_animation_step_time_.value());
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
    if (SidePanelUI* side_panel_ui =
            browser_view_->browser()->GetFeatures().side_panel_ui()) {
      if (std::optional<SidePanelEntry::Id> entry =
              side_panel_ui->GetCurrentEntryId(type_)) {
        std::string current_panel_id = SidePanelEntryIdToString(entry.value());
        // Update the pref with the new width.
        UpdateSidePanelWidthPref(current_panel_id, proposed_width);
      }
    }

    SetPanelWidth(proposed_width);
    did_resize_ = true;
  }
}

void SidePanel::RecordMetricsIfResized() {
  if (did_resize_) {
    SidePanelUI* side_panel_ui =
        browser_view_->browser()->GetFeatures().side_panel_ui();
    if (!side_panel_ui) {
      return;
    }
    std::optional<SidePanelEntry::Id> id =
        side_panel_ui->GetCurrentEntryId(type_);
    if (!id.has_value()) {
      return;
    }

    int side_panel_contents_width = width() - GetBorderInsets().width();
    int browser_window_width = browser_view_->width();
    SidePanelUtil::RecordSidePanelResizeMetrics(
        type_, id.value(), side_panel_contents_width, browser_window_width);
    did_resize_ = false;
  }
}

void SidePanel::Open(bool animated) {
  UpdateVisibility(/*should_be_open=*/true, animated);
}

void SidePanel::Close(bool animated) {
  UpdateVisibility(/*should_be_open=*/false, animated);
}

void SidePanel::ResetSidePanelAnimationContent() {
  if (browser_view_->GetSidePanelAnimationContent()) {
    CHECK(content_parent_view_->children().size() == 0);
    content_parent_view_->AddChildView(
        browser_view_->GetSidePanelAnimationContent());
    browser_view_->SetSidePanelAnimationContent(nullptr);
    animation_coordinator_->Reset(SidePanelAnimationCoordinator::AnimationType::
                                      kOpenWithContentTransition);
  }
}

views::View* SidePanel::GetContentParentView() {
  return content_parent_view_;
}

void SidePanel::UpdateVisibility(bool should_be_open, bool animate_transition) {
  animate_transition &= ShouldShowAnimation();
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
  if (border_view_ &&
      side_panel_open_or_closing != border_view_->GetVisible()) {
    border_view_->SetVisible(side_panel_open_or_closing);
    if (side_panel_open_or_closing) {
      border_view_->SetPaintToLayer();
      border_view_->layer()->SetFillsBoundsOpaquely(false);
      if (header_view_ && header_view_->GetVisible()) {
        border_view_->HeaderViewChanged(header_view_);
        int top_inset = header_view_->height() - GetBorderInsets().top();
        SetBorder(views::CreateEmptyBorder(
            GetBorderInsets() + gfx::Insets::TLBR(top_inset, 0, 0, 0)));
      }
    } else {
      border_view_->DestroyLayer();
    }
  }
  if (animate_transition) {
    if (should_be_open) {
      // If the side panel should remain open but there are views to hide, hide
      // them immediately.
      for (auto* view : views_to_hide) {
        view->SetVisible(false);
      }
      SetVisible(should_be_open);
      largest_animation_step_time_.reset();
      last_animation_step_timestamp_ = base::TimeTicks::Now();
      if (content_starting_bounds_.has_value()) {
        CHECK(content_parent_view_->children().size() == 1);
        browser_view_->SetSidePanelAnimationContent(
            content_parent_view_->children()[0]);
        animation_coordinator_->Start(
            SidePanelAnimationCoordinator::AnimationType::
                kOpenWithContentTransition);
      } else {
        animation_coordinator_->Start(
            SidePanelAnimationCoordinator::AnimationType::kOpen);
      }
    } else if (GetVisible() && !IsClosing()) {
      animation_coordinator_->Start(
          SidePanelAnimationCoordinator::AnimationType::kClose);
    }
  } else {
    // Set the animation value so that it accurately reflects what state the
    // side panel should be in for layout.
    animation_coordinator_->Reset(
        should_be_open ? SidePanelAnimationCoordinator::AnimationType::kOpen
                       : SidePanelAnimationCoordinator::AnimationType::kClose);

    SetVisible(should_be_open);
  }
}

double SidePanel::GetAnimationValueFor(
    const SidePanelAnimationCoordinator::SidePanelAnimationId& animation_id)
    const {
  if (ShouldShowAnimation()) {
    return animation_coordinator_->GetAnimationValueFor(animation_id);
  } else {
    return 1;
  }
}

bool SidePanel::ShouldShowAnimation() const {
  return gfx::Animation::ShouldRenderRichAnimation() && !animations_disabled_;
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

void SidePanel::UpdateHorizontalAlignment() {
  horizontal_alignment_ =
      GetHorizontalAlignment(browser_view_->GetProfile()->GetPrefs(), type_);

  InvalidateLayout();
}

BEGIN_METADATA(SidePanel)
END_METADATA
