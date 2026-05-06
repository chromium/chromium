// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_metrics.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/animations/side_panel_animations.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "chrome/browser/ui/views/side_panel/side_panel_resize_area.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/interaction/element_tracker.h"
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
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"

namespace {
constexpr BrowserAnimationGroup kAnimationGroup =
    SidePanelAnimations::kSidePanel;

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
    bool use_default_horizontal_alignment) {
  bool is_right_aligned =
      pref_service->GetBoolean(prefs::kSidePanelHorizontalAlignment);
  is_right_aligned =
      use_default_horizontal_alignment ? is_right_aligned : !is_right_aligned;
  return is_right_aligned ? SidePanel::HorizontalAlignment::kRight
                          : SidePanel::HorizontalAlignment::kLeft;
}

class ContentParentBackground : public views::Background {
 public:
  ContentParentBackground(BrowserView* browser_view,
                          base::RepeatingCallback<gfx::RoundedCornersF()>
                              get_rounded_corners_callback)
      : browser_view_(browser_view),
        get_rounded_corners_callback_(std::move(get_rounded_corners_callback)) {
  }

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::RoundedCornersF radii = get_rounded_corners_callback_.Run();
    SkVector sk_radii[4] = {{radii.upper_left(), radii.upper_left()},
                            {radii.upper_right(), radii.upper_right()},
                            {radii.lower_right(), radii.lower_right()},
                            {radii.lower_left(), radii.lower_left()}};
    SkRRect rrect;
    rrect.setRectRadii(gfx::RectToSkRect(view->GetLocalBounds()), sk_radii);
    SkPath path = SkPath::RRect(rrect);
    canvas->ClipPath(path, /*do_anti_alias=*/true);

      ThemedBackground::PaintBackground(canvas, view, browser_view_);
  }

 private:
  const raw_ptr<BrowserView> browser_view_;
  base::RepeatingCallback<gfx::RoundedCornersF()> get_rounded_corners_callback_;
};

// ContentParentView is the parent view for views hosted in the
// side panel.
class ContentParentView : public views::View, public views::ViewObserver {
  METADATA_HEADER(ContentParentView, views::View)

 public:
  explicit ContentParentView(BrowserView* browser_view)
      : browser_view_(browser_view) {
    SetUseDefaultFillLayout(true);
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    // Observe this view to round corners of children as they are added.
    view_observation_.Observe(this);
  }

  ~ContentParentView() override = default;

 private:
  void AddedToWidget() override {
    SetBackground(std::make_unique<ContentParentBackground>(
        browser_view_,
        base::BindRepeating(&ContentParentView::GetRoundedCorners,
                            base::Unretained(this))));
  }

  void OnChildViewAdded(views::View* observed_view,
                        views::View* child) override {
    // Setting rounded corners on a WebView's holder requires the
    // NativeViewHost's native_wrapper_ to be set. For a WebView, this is set in
    // View::ViewHierarchyChanged, specifically after
    // WebView::AttachWebContentsNativeView is called. Therefore, to round the
    // corners initially, set them in OnChildViewAdded, which is called after
    // View::ViewHierarchyChanged.
    // If the child is a WebView or paints to a layer, round its corners.
    if (views::IsViewClass<views::WebView>(child)) {
      views::AsViewClass<views::WebView>(child)->holder()->SetCornerRadii(
          GetRoundedCorners());
    }
    // Try to detect if the child is a views::View wrapper of a WebView. If so,
    // round its corners.
    if (child->children().size() == 1 &&
        views::IsViewClass<views::WebView>(child->children()[0])) {
      views::AsViewClass<views::WebView>(child->children()[0])
          ->holder()
          ->SetCornerRadii(GetRoundedCorners());
    }
    if (child->layer()) {
      child->layer()->SetIsFastRoundedCorner(true);
      child->layer()->SetRoundedCornerRadius(GetRoundedCorners());
    }
  }

  void OnWebContentsAttached(views::WebView* web_view) {
    CHECK(web_view);
    CHECK(web_view->holder());

    // Native View Host doesn't always get reused, so ensure a nested Native
    // View's corners are always rounded.
    web_view->holder()->SetCornerRadii(GetRoundedCorners());
    // Temporary subscription. We only need to round the corners once.
    web_contents_attached_callback_ = {};
  }

  void OnViewHierarchyChanged(
      views::View* observed_view,
      const views::ViewHierarchyChangedDetails& details) override {
    // TODO(b/490050482): Certain features that provide a wrapping parent View
    // can swap the child View in-place without triggering OnChildViewAdded or
    // OnChildViewRemoved, causing non-rounded corners on the child after
    // the swap occurs (reference b/344626785).
    if (!views::IsViewClass<views::WebView>(details.child) ||
        details.parent == this) {
      return;
    }
    web_contents_attached_callback_ =
        views::AsViewClass<views::WebView>(details.child)
            ->AddWebContentsAttachedCallback(
                base::BindRepeating(&ContentParentView::OnWebContentsAttached,
                                    base::Unretained(this)));
  }

  gfx::RoundedCornersF GetRoundedCorners() {
    ChromeDistanceMetric corner_radius =
        ChromeDistanceMetric::DISTANCE_SIDE_PANEL_CONTENT_RADIUS;
    return GetLayoutProvider()
               ? gfx::RoundedCornersF(
                     GetLayoutProvider()->GetDistanceMetric(corner_radius))
               : gfx::RoundedCornersF();
  }

  const raw_ptr<BrowserView> browser_view_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
  base::CallbackListSubscription web_contents_attached_callback_;
};

BEGIN_METADATA(ContentParentView)
END_METADATA

}  // namespace

DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(SidePanel, kOpenAnimationCompletedEvent);
DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(SidePanel,
                                       kCloseAnimationCompletedEvent);

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

SidePanel::SidePanel(BrowserView* browser_view)
    : browser_view_(browser_view),
      visible_bounds_view_clipper_(
          std::make_unique<VisibleBoundsViewClipper>(this)) {
  horizontal_alignment_ =
      GetHorizontalAlignment(browser_view->GetProfile()->GetPrefs(),
                             use_default_horizontal_alignment_);

  // The default z-order is the order in which children were added to the
  // parent view. content_parent_view_ is added first so it exists behind
  // the resize_area_.
  content_parent_view_ =
      AddChildView(std::make_unique<ContentParentView>(browser_view));
  content_parent_view_->SetVisible(false);

  resize_area_ =
      AddChildView(std::make_unique<views::SidePanelResizeArea>(this));
  resize_area_->InsertBeforeInFocusList(content_parent_view_);

  pref_change_registrar_.Init(browser_view->GetProfile()->GetPrefs());

  pref_change_registrar_.Add(
      prefs::kSidePanelHorizontalAlignment,
      base::BindRepeating(&SidePanel::UpdateHorizontalAlignment,
                          base::Unretained(this)));

  animation_subscription_ =
      BrowserAnimationController::From(browser_view_->browser())
          ->Subscribe(kAnimationGroup,
                      base::BindRepeating(&SidePanel::OnAnimationProgressed,
                                          base::Unretained(this)));

  SetVisible(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Set the panel width from the preference or use the minimum size as the
  // default.
  SetPanelWidth(GetMinimumSize().width());

  SetProperty(views::kElementIdentifierKey, kSidePanelElementId);
}

SidePanel::~SidePanel() = default;

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
}

void SidePanel::UpdateWidthOnEntryChanged() {
  SidePanelUI* side_panel_ui =
      browser_view_->browser()->GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return;
  }

  std::optional<SidePanelEntry::Id> current_entry =
      side_panel_ui->GetCurrentEntryId();
  if (!current_entry) {
    return;
  }

  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  const base::DictValue& dict =
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
    SetPanelWidth(side_panel_ui->GetCurrentEntryDefaultContentWidth() +
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
  return SidePanelAnimations::kClose ==
         BrowserAnimationController::From(browser_view_->browser())
             ->GetCurrentMotion(kAnimationGroup);
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
      GetAnimationValueFor(SidePanelAnimations::kContentTop);
  animating_bounds.set_y(gfx::Tween::IntValueBetween(
      top_bound_animation_value, content_starting_bounds.y(),
      final_bounds.y()));
  double bottom_bound_animation_value =
      GetAnimationValueFor(SidePanelAnimations::kContentBottom);
  animating_bounds.set_height(
      gfx::Tween::IntValueBetween(bottom_bound_animation_value,
                                  content_starting_bounds.bottom(),
                                  final_bounds.bottom()) -
      animating_bounds.y());
  double left_bound_animation_value =
      GetAnimationValueFor(SidePanelAnimations::kContentLeft);
  animating_bounds.set_x(gfx::Tween::IntValueBetween(
      left_bound_animation_value, content_starting_bounds.x(),
      final_bounds.x()));
  double width_bound_animation_value =
      GetAnimationValueFor(SidePanelAnimations::kContentWidth);
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
  // Update the border so that the insets include space for the header to be
  // placed on top of the border area.
  int top_inset = header_view_->height() - GetBorderInsets().top();
  SetBorder(views::CreateEmptyBorder(GetBorderInsets() +
                                     gfx::Insets::TLBR(top_inset, 0, 0, 0)));
}

void SidePanel::RemoveHeaderView() {
  SetBorder(views::CreateEmptyBorder(GetBorderInsets().set_top(0)));
  if (header_view_) {
    auto header_view = RemoveChildViewT(header_view_);
    header_view_ = nullptr;
  }
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
  return GetAnimationValueFor(SidePanelAnimations::kPanelWidth);
}

void SidePanel::OnAnimationProgressed(
    const BrowserAnimationController* controller,
    BrowserAnimationUpdate status) {
  switch (status) {
    case BrowserAnimationUpdate::kStarted:
      break;
    case BrowserAnimationUpdate::kProgressed:
      if (const auto width = controller->GetCurrentValue(
              kAnimationGroup, SidePanelAnimations::kPanelWidth)) {
        if (last_animation_values_[SidePanelAnimations::kPanelWidth] !=
            *width) {
          last_animation_values_[SidePanelAnimations::kPanelWidth] = *width;
          InvalidateLayout();
        }
      }
      if (const auto content_left = controller->GetCurrentValue(
              kAnimationGroup, SidePanelAnimations::kContentLeft)) {
        if (last_animation_values_[SidePanelAnimations::kContentLeft] !=
            *content_left) {
          last_animation_values_[SidePanelAnimations::kContentLeft] =
              *content_left;
          browser_view_->InvalidateLayout();
        }
      }
      break;
    case BrowserAnimationUpdate::kEnded: {
      const auto motion = controller->GetCurrentMotion(kAnimationGroup);
      if (motion == SidePanelAnimations::kClose) {
        state_ = State::kClosed;
        views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
            kCloseAnimationCompletedEvent, this);
        SetVisible(false);
      } else if (motion) {
        if (motion == SidePanelAnimations::kOpenWithContentTransition) {
          if (browser_view_->GetSidePanelAnimationContent()) {
            content_parent_view_->AddChildView(
                browser_view_->GetSidePanelAnimationContent());
            browser_view_->SetSidePanelAnimationContent(nullptr);
          }
          content_starting_bounds_.reset();
        }
        state_ = State::kOpen;
        views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
            kOpenAnimationCompletedEvent, this);
      }
      last_animation_values_.clear();
      break;
    }
    case BrowserAnimationUpdate::kCanceled:
      last_animation_values_.clear();
      break;
  }
}

void SidePanel::UpdateSidePanelWidthPref(const std::string& panel_id,
                                         int width) {
  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  ScopedDictPrefUpdate update(pref_service, prefs::kSidePanelIdToWidth);
  base::DictValue& dict = update.Get();

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
              side_panel_ui->GetCurrentEntryId()) {
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
    std::optional<SidePanelEntry::Id> id = side_panel_ui->GetCurrentEntryId();
    if (!id.has_value()) {
      return;
    }

    int side_panel_contents_width = width() - GetBorderInsets().width();
    int browser_window_width = browser_view_->width();
    SidePanelMetrics::RecordSidePanelResizeMetrics(
        id.value(), side_panel_contents_width, browser_window_width);
    did_resize_ = false;
  }
}

void SidePanel::SetCurrentEntryType(SidePanelType type) {
  if (current_entry_type_ != type) {
    current_entry_type_ = type;
  }
}

SidePanelType SidePanel::GetCurrentEntryType() const {
  return current_entry_type_;
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
    content_starting_bounds_.reset();
    content_parent_view_->AddChildView(
        browser_view_->GetSidePanelAnimationContent());
    browser_view_->SetSidePanelAnimationContent(nullptr);
    auto* const controller =
        BrowserAnimationController::From(browser_view_->browser());
    controller->Clear(kAnimationGroup);
  }
}

void SidePanel::SetActiveEntryUsesDefaultHorizontalAlignment(
    bool use_default_horizontal_alignment) {
  if (use_default_horizontal_alignment_ == use_default_horizontal_alignment) {
    return;
  }
  use_default_horizontal_alignment_ = use_default_horizontal_alignment;
  UpdateHorizontalAlignment();
}

views::View* SidePanel::GetContentParentView() {
  return content_parent_view_;
}

void SidePanel::UpdateVisibility(bool should_be_open, bool animate_transition) {
  animate_transition &= ShouldShowAnimation();
  if (should_be_open) {
    state_ = animate_transition && state_ != State::kOpen ? State::kOpening
                                                          : State::kOpen;
  } else {
    state_ = animate_transition && state_ != State::kClosed ? State::kClosing
                                                            : State::kClosed;
  }
  std::vector<views::View*> views_to_hide;
  // TODO(pbos): Iterate content instead. Requires moving the owned pointer out
  // of owned contents before resetting it.
  for (views::View* view : children()) {
    if (view == resize_area_ || view == header_view_ || !view->GetVisible()) {
      continue;
    }

    if (state_ == State::kClosing || state_ == State::kClosed) {
      views_to_hide.push_back(view);
    }
  }
  auto* const animation_controller =
      BrowserAnimationController::From(browser_view_->browser());
  if (animate_transition) {
    BrowserAnimationMotion motion;
    if (should_be_open) {
      // If the side panel should remain open but there are views to hide, hide
      // them immediately.
      for (auto* view : views_to_hide) {
        view->SetVisible(false);
      }
      SetVisible(should_be_open);
      if (content_starting_bounds_.has_value()) {
        CHECK(content_parent_view_->children().size() == 1);
        browser_view_->SetSidePanelAnimationContent(
            content_parent_view_->children()[0]);
        motion = SidePanelAnimations::kOpenWithContentTransition;
      } else if (state_ != State::kOpen) {
        motion = SidePanelAnimations::kOpen;
      }
    } else if (GetVisible() && !IsClosing()) {
      motion = SidePanelAnimations::kClose;
    }
    if (motion) {
      animation_controller->Start(
          kAnimationGroup, motion,
          /*group_histogram_override=*/
          current_entry_type_ == SidePanelType::kToolbar
              ? SidePanelMetrics::kSidePanelToolbarHeightHistogramName
              : SidePanelMetrics::kSidePanelHistogramName);
    }
  } else {
    animation_controller->Clear(kAnimationGroup);
    SetVisible(should_be_open);
  }
}

double SidePanel::GetAnimationValueFor(BrowserAnimationSequence which) const {
  return BrowserAnimationController::From(browser_view_->browser())
      ->GetCurrentValue(kAnimationGroup, which)
      .value_or(state_ == State::kOpen ? 1.0 : 0.0);
}

bool SidePanel::ShouldShowAnimation() const {
  bool should_show_animations =
      gfx::Animation::ShouldRenderRichAnimation() && !animations_disabled_;
  return should_show_animations;
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
      GetHorizontalAlignment(browser_view_->GetProfile()->GetPrefs(),
                             use_default_horizontal_alignment_);

  InvalidateLayout();
}

BEGIN_METADATA(SidePanel)
END_METADATA
