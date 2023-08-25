// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_resize_area.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/pref_names.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
constexpr int kBorderThickness = 16 + views::Separator::kThickness;

// This is how many units of the toolbar are essentially expected to be
// background.
constexpr int kOverlapFromToolbar = 4;

// We want the border to visually look like kBorderThickness units on all sides.
// On the top side, background is drawn on top of the top-content separator and
// some units of background inside the toolbar (or bookmarks bar) itself.
// Subtract both of those to not get visually-excessive padding.
constexpr auto kBorderInsets = gfx::Insets::TLBR(
    kBorderThickness - views::Separator::kThickness - kOverlapFromToolbar,
    kBorderThickness,
    kBorderThickness,
    kBorderThickness);

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

    const SkScalar radii[8] = {
        border_radii_.upper_left(),  border_radii_.upper_left(),
        border_radii_.upper_right(), border_radii_.upper_right(),
        border_radii_.lower_right(), border_radii_.lower_right(),
        border_radii_.lower_left(),  border_radii_.lower_left()};

    // Use ToEnclosedRect to make sure that `rounded_border_path` never end up
    // larger than the view bounds.
    const gfx::Rect scaled_view_bounds = ToEnclosedRect(scaled_view_bounds_f);

    SkPath rounded_border_path;
    rounded_border_path.addRoundRect(gfx::RectToSkRect(scaled_view_bounds),
                                     radii, SkPathDirection::kCW);

    // Add another clip to the canvas that rounds the outer corners of the
    // border.
    canvas->ClipPath(rounded_border_path, /*do_anti_alias=*/true);

    // Draw the top-container background.
    {
      // Redo device-scale factor, the theme background is drawn in DIPs. Note
      // that the clip area above is in pixels, hence the
      // UndoDeviceScaleFactor() call before this.
      gfx::ScopedCanvas scoped_rescale(canvas);
      canvas->Scale(dsf, dsf);

      TopContainerBackground::PaintBackground(canvas, &view, browser_view_);
    }

    // Paint the inner border around SidePanel content.
    const float stroke_thickness = views::Separator::kThickness * dsf;

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
    // inset is outside the SidePanel itself, but not outside the BorderView.
    // If there is a header we want to increase the top inset to give room for
    // the header to paint on top of the border area.
    return kBorderInsets +
           gfx::Insets::TLBR(views::Separator::kThickness + header_height_, 0,
                             0, 0);
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

  void Layout() override {
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

}  // namespace

SidePanel::SidePanel(BrowserView* browser_view,
                     HorizontalAlignment horizontal_alignment)
    : border_view_(AddChildView(std::make_unique<BorderView>(browser_view))),
      browser_view_(browser_view),
      resize_area_(
          AddChildView(std::make_unique<views::SidePanelResizeArea>(this))),
      horizontal_alignment_(horizontal_alignment) {
  pref_change_registrar_.Init(browser_view->GetProfile()->GetPrefs());

  // base::Unretained is safe since the side panel must be attached to some
  // BrowserView. Deleting BrowserView will also delete the SidePanel.
  pref_change_registrar_.Add(
      prefs::kSidePanelHorizontalAlignment,
      base::BindRepeating(&BrowserView::UpdateSidePanelHorizontalAlignment,
                          base::Unretained(browser_view)));

  SetVisible(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // TODO(pbos): Reconsider if SetPanelWidth() should add borders, if so move
  // accounting for the border into SetPanelWidth(), otherwise remove this TODO.
  SetPanelWidth(GetMinimumSize().width());

  SetBorder(views::CreateEmptyBorder(kBorderInsets));

  SetProperty(views::kElementIdentifierKey, kSidePanelElementId);

  AddObserver(this);
}

SidePanel::~SidePanel() {
  RemoveObserver(this);
}

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

void SidePanel::SetHorizontalAlignment(HorizontalAlignment alignment) {
  horizontal_alignment_ = alignment;
}

SidePanel::HorizontalAlignment SidePanel::GetHorizontalAlignment() {
  return horizontal_alignment_;
}

bool SidePanel::IsRightAligned() {
  return GetHorizontalAlignment() == kAlignRight;
}

gfx::Size SidePanel::GetMinimumSize() const {
  const int min_side_panel_contents_width = 320;
  const int min_height = 0;
  return gfx::Size(min_side_panel_contents_width + kBorderInsets.width(),
                   min_height);
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
  SetBorder(views::CreateEmptyBorder(
      kBorderInsets + gfx::Insets::TLBR(header_view_->height(), 0, 0, 0)));
}

gfx::Size SidePanel::GetContentSizeUpperBound() const {
  const int side_panel_width = width() > 0 ? width() : GetMinimumSize().width();
  const int side_panel_height =
      height() > 0 ? height() : browser_view_->height();

  return gfx::Size(std::max(0, side_panel_width - kBorderInsets.width()),
                   std::max(0, side_panel_height - kBorderInsets.height()));
}

void SidePanel::ChildVisibilityChanged(View* child) {
  UpdateVisibility();
}

void SidePanel::OnChildViewAdded(View* observed_view, View* child) {
  UpdateVisibility();
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
  UpdateVisibility();
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
    SetPanelWidth(proposed_width);
    did_resize_ = true;
  }
}

void SidePanel::RecordMetricsIfResized() {
  if (did_resize_) {
    absl::optional<SidePanelEntry::Id> id =
        SidePanelUI::GetSidePanelUIForBrowser(browser_view_->browser())
            ->GetCurrentEntryId();
    CHECK(id.has_value());
    int side_panel_contents_width = width() - kBorderInsets.width();
    int browser_window_width = browser_view_->width();
    SidePanelUtil::RecordSidePanelResizeMetrics(
        id.value(), side_panel_contents_width, browser_window_width);
    did_resize_ = false;
  }
}

void SidePanel::UpdateVisibility() {
  bool any_child_visible = false;
  // TODO(pbos): Iterate content instead. Requires moving the owned pointer out
  // of owned contents before resetting it.
  for (const auto* view : children()) {
    if (view == border_view_ || view == resize_area_ || view == header_view_) {
      continue;
    }

    if (view->GetVisible()) {
      any_child_visible = true;
      break;
    }
  }
  // Make sure the border visibility matches the side panel. Also dynamically
  // create and destroy the layer to reclaim memory and avoid painting and
  // compositing this border when it's not showing. See
  // https://crbug.com/1269090.
  // TODO(pbos): Should layer visibility/painting be automatically tied to
  // parent visibility? I.e. the difference between GetVisible() and IsDrawn().
  if (any_child_visible != border_view_->GetVisible()) {
    border_view_->SetVisible(any_child_visible);
    if (any_child_visible) {
      border_view_->SetPaintToLayer();
      border_view_->layer()->SetFillsBoundsOpaquely(false);
      if (header_view_) {
        static_cast<BorderView*>(border_view_)->HeaderViewChanged(header_view_);
        SetBorder(views::CreateEmptyBorder(
            kBorderInsets +
            gfx::Insets::TLBR(header_view_->height(), 0, 0, 0)));
      }
    } else {
      border_view_->DestroyLayer();
    }
  }
  SetVisible(any_child_visible);
}

BEGIN_METADATA(SidePanel, views::View)
END_METADATA
