// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/commerce/product_specifications_button.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_scroll_container.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "tab_strip_region_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/border.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/metrics/histogram_functions.h"
#endif

namespace {

class FrameGrabHandle : public views::View {
  METADATA_HEADER(FrameGrabHandle, views::View)

 public:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Reserve some space for the frame to be grabbed by, even if the tabstrip
    // is full.
    // TODO(tbergquist): Define this relative to the NTB insets again.
    return gfx::Size(42, 0);
  }
};

BEGIN_METADATA(FrameGrabHandle)
END_METADATA

bool ShouldShowNewTabButton(const Browser* browser) {
  // `browser` can be null in tests and `app_controller` will be null if
  // the browser is not for an app.
  return !browser || !browser->app_controller() ||
         !browser->app_controller()->ShouldHideNewTabButton();
}

}  // namespace

TabStripRegionView::TabStripRegionView(std::unique_ptr<TabStrip> tab_strip)
    : profile_(tab_strip->GetBrowser() ? tab_strip->GetBrowser()->profile()
                                       : nullptr),
      render_tab_search_before_tab_strip_(
          !tabs::GetTabSearchTrailingTabstrip(profile_)),
      tab_search_position_metrics_logger_(
          std::make_unique<TabSearchPositionMetricsLogger>(profile_)) {
  views::SetCascadingColorProviderColor(
      this, views::kCascadingBackgroundColor,
      kColorTabBackgroundInactiveFrameInactive);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  GetViewAccessibility().SetIsMultiselectable(true);

  tab_strip_ = tab_strip.get();
  const Browser* browser = tab_strip_->GetBrowser();

  // Add and configure the TabSearchContainer.
  std::unique_ptr<TabSearchContainer> tab_search_container;
  if (browser && browser->is_type_normal()) {
    tab_search_container = std::make_unique<TabSearchContainer>(
        tab_strip_->controller(), browser->tab_strip_model(),
        render_tab_search_before_tab_strip_, this,
        browser->browser_window_features()->tab_declutter_controller());
    tab_search_container->SetProperty(views::kCrossAxisAlignmentKey,
                                      views::LayoutAlignment::kCenter);
  }

  // Add and configure the ProductSpecificationsButton.
  std::unique_ptr<ProductSpecificationsButton> product_specifications_button;
  if (tab_search_container &&
      base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
    product_specifications_button =
        std::make_unique<ProductSpecificationsButton>(
            tab_strip_->controller(), browser->tab_strip_model(),
            browser->browser_window_features()
                ->product_specifications_entry_point_controller(),
            render_tab_search_before_tab_strip_, this);
    product_specifications_button->SetProperty(views::kCrossAxisAlignmentKey,
                                               views::LayoutAlignment::kCenter);
  }

  if (tab_search_container && render_tab_search_before_tab_strip_) {
    tab_search_container->SetPaintToLayer();
    tab_search_container->layer()->SetFillsBoundsOpaquely(false);

    tab_search_container_ = AddChildView(std::move(tab_search_container));

    // Inset between the tabsearch and tabstrip should be reduced to account for
    // extra spacing.
    tab_search_container_->SetProperty(views::kViewIgnoredByLayoutKey, true);

    if (product_specifications_button) {
      product_specifications_button->SetPaintToLayer();
      product_specifications_button->layer()->SetFillsBoundsOpaquely(false);

      product_specifications_button_ =
          AddChildView(std::move(product_specifications_button));
      product_specifications_button_->SetProperty(
          views::kViewIgnoredByLayoutKey, true);
    }
  }

  if (base::FeatureList::IsEnabled(tabs::kScrollableTabStrip)) {
    std::unique_ptr<TabStripScrollContainer> scroll_container =
        std::make_unique<TabStripScrollContainer>(std::move(tab_strip));
    tab_strip_scroll_container_ = scroll_container.get();
    tab_strip_container_ = AddChildView(std::move(scroll_container));
    // Allow the |tab_strip_container_| to grow into the free space available in
    // the TabStripRegionView.
    const views::FlexSpecification tab_strip_container_flex_spec =
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred);
    tab_strip_container_->SetProperty(views::kFlexBehaviorKey,
                                      tab_strip_container_flex_spec);

  } else {
    tab_strip_container_ = AddChildView(std::move(tab_strip));

    // Allow the |tab_strip_container_| to grow into the free space available in
    // the TabStripRegionView.
    const views::FlexSpecification tab_strip_container_flex_spec =
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred);
    tab_strip_container_->SetProperty(views::kFlexBehaviorKey,
                                      tab_strip_container_flex_spec);
  }

  if (ShouldShowNewTabButton(browser)) {
    std::unique_ptr<TabStripControlButton> tab_strip_control_button =
        std::make_unique<TabStripControlButton>(
            tab_strip_->controller(),
            base::BindRepeating(&TabStrip::NewTabButtonPressed,
                                base::Unretained(tab_strip_)),
            vector_icons::kAddIcon);
    tab_strip_control_button->SetProperty(views::kElementIdentifierKey,
                                          kNewTabButtonElementId);

    new_tab_button_ = AddChildView(std::move(tab_strip_control_button));

    new_tab_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));
    new_tab_button_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_NEWTAB));

    // TODO(crbug.com/40118868): Revisit the macro expression once build flag
    // switch of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    // The New Tab Button can be middle-clicked on Linux.
    new_tab_button_->SetTriggerableEventFlags(
        new_tab_button_->GetTriggerableEventFlags() |
        ui::EF_MIDDLE_MOUSE_BUTTON);
#endif
  }

  reserved_grab_handle_space_ =
      AddChildView(std::make_unique<FrameGrabHandle>());
  reserved_grab_handle_space_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(3));

  SetProperty(views::kElementIdentifierKey, kTabStripRegionElementId);

  if (browser && tab_search_container && !render_tab_search_before_tab_strip_) {
    if (product_specifications_button) {
      product_specifications_button_ =
          AddChildView(std::move(product_specifications_button));
    }
    tab_search_container_ = AddChildView(std::move(tab_search_container));
    tab_search_container_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0, GetLayoutConstant(TAB_STRIP_PADDING)));
  }

  UpdateTabStripMargin();
}

TabStripRegionView::~TabStripRegionView() = default;

bool TabStripRegionView::IsRectInWindowCaption(const gfx::Rect& rect) {
  const auto get_target_rect = [&](views::View* target) {
    gfx::RectF rect_in_target_coords_f(rect);
    View::ConvertRectToTarget(this, target, &rect_in_target_coords_f);
    return gfx::ToEnclosingRect(rect_in_target_coords_f);
  };

  // Perform checks for buttons that should be rendered above the tabstrip.
  if (new_tab_button_ && new_tab_button_->GetLocalBounds().Intersects(
                             get_target_rect(new_tab_button_))) {
    return !new_tab_button_->HitTestRect(get_target_rect(new_tab_button_));
  }

  if (render_tab_search_before_tab_strip_ && tab_search_container_ &&
      tab_search_container_->GetLocalBounds().Intersects(
          get_target_rect(tab_search_container_))) {
    return !tab_search_container_->HitTestRect(
        get_target_rect(tab_search_container_));
  }

  if (render_tab_search_before_tab_strip_ && product_specifications_button_ &&
      product_specifications_button_->GetLocalBounds().Intersects(
          get_target_rect(product_specifications_button_))) {
    return !product_specifications_button_->HitTestRect(
        get_target_rect(product_specifications_button_));
  }

  // Perform a hit test against the |tab_strip_container_| to ensure that the
  // rect is within the visible portion of the |tab_strip_| before calling the
  // tab strip's |IsRectInWindowCaption()| for scrolling disabled. Defer to
  // scroll container if scrolling is enabled.
  // TODO(tluk): Address edge case where |rect| might partially intersect with
  // the |tab_strip_container_| and the |tab_strip_| but not over the same
  // pixels. This could lead to this returning false when it should be returning
  // true.
  if (tab_strip_container_->HitTestRect(
          get_target_rect(tab_strip_container_))) {
    if (base::FeatureList::IsEnabled(tabs::kScrollableTabStrip)) {
      TabStripScrollContainer* scroll_container =
          views::AsViewClass<TabStripScrollContainer>(tab_strip_container_);

      return scroll_container->IsRectInWindowCaption(
          get_target_rect(scroll_container));

    } else {
      return tab_strip_->IsRectInWindowCaption(get_target_rect(tab_strip_));
    }
  }

  // The child could have a non-rectangular shape, so if the rect is not in the
  // visual portions of the child view we treat it as a click to the caption.
  for (View* const child : children()) {
    if (child != tab_strip_container_ && child != reserved_grab_handle_space_ &&
        child->GetLocalBounds().Intersects(get_target_rect(child))) {
      return !child->HitTestRect(get_target_rect(child));
    }
  }

#if BUILDFLAG(IS_WIN)
  bool rect_in_reserved_space =
      reserved_grab_handle_space_->GetLocalBounds().Intersects(
          get_target_rect(reserved_grab_handle_space_));
  ReportCaptionHitTestInReservedGrabHandleSpace(rect_in_reserved_space);
#endif

  return true;
}

bool TabStripRegionView::IsPositionInWindowCaption(const gfx::Point& point) {
  return IsRectInWindowCaption(gfx::Rect(point, gfx::Size(1, 1)));
}

views::View::Views TabStripRegionView::GetChildrenInZOrder() {
  views::View::Views children;

  if (tab_strip_container_) {
    children.emplace_back(tab_strip_container_.get());
  }

  if (new_tab_button_) {
    children.emplace_back(new_tab_button_.get());
  }

  if (tab_search_container_) {
    children.emplace_back(tab_search_container_.get());
  }

  if (product_specifications_button_) {
    children.emplace_back(product_specifications_button_.get());
  }

  if (reserved_grab_handle_space_) {
    children.emplace_back(reserved_grab_handle_space_.get());
  }

  return children;
}

// The TabSearchButton need bounds that overlap the TabStripContainer, which
// FlexLayout doesn't currently support. Because of this the TSB bounds are
// manually calculated.
void TabStripRegionView::Layout(PassKey) {
  const bool tab_search_container_before_tab_strip =
      tab_search_container_ && render_tab_search_before_tab_strip_;
  if (tab_search_container_before_tab_strip) {
    UpdateTabStripMargin();
  }

  LayoutSuperclass<views::AccessiblePaneView>(this);

  if (tab_search_container_before_tab_strip) {
    // Manually adjust x-axis position of the UI components. Currently the
    // components are `tab_search_container_` and
    // `product_specifications_button` if it's available.
    if (product_specifications_button_) {
      AdjustViewBoundsRect(product_specifications_button_, 0);
    }

    int product_specifications_button_width =
        product_specifications_button_
            ? product_specifications_button_->GetPreferredSize().width()
            : 0;
    AdjustViewBoundsRect(tab_search_container_,
                         product_specifications_button_width);
  }

  if (new_tab_button_) {
    // The NTB needs to be layered on top of the tabstrip to achieve negative
    // margins.
    gfx::Size new_tab_button_size = new_tab_button_->GetPreferredSize();

    // The y position is measured from the bottom of the tabstrip, and then
    // padding and button height are removed.
    int x = tab_strip_container_->bounds().right() -
            TabStyle::Get()->GetBottomCornerRadius() +
            GetLayoutConstant(TAB_STRIP_PADDING);

    if (base::FeatureList::IsEnabled(features::kCompactMode)) {
      if (profile_->GetPrefs()->GetBoolean(prefs::kCompactModeEnabled)) {
        x -= GetLayoutConstant(TAB_STRIP_PADDING);
      }
    }
    gfx::Point new_tab_button_new_position = gfx::Point(x, 0);

    gfx::Rect new_tab_button_new_bounds =
        gfx::Rect(new_tab_button_new_position, new_tab_button_size);

    // If the tabsearch button is before the tabstrip container, then manually
    // set the bounds.
    new_tab_button_->SetBoundsRect(new_tab_button_new_bounds);
  }
}

bool TabStripRegionView::CanDrop(const OSExchangeData& data) {
  return TabDragController::IsSystemDragAndDropSessionRunning() &&
         data.HasCustomFormat(
             ui::ClipboardFormatType::GetType(ui::kMimeTypeWindowDrag));
}

bool TabStripRegionView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(
      ui::ClipboardFormatType::GetType(ui::kMimeTypeWindowDrag));
  return true;
}

void TabStripRegionView::OnDragEntered(const ui::DropTargetEvent& event) {
  CHECK(TabDragController::IsSystemDragAndDropSessionRunning());
  TabDragController::OnSystemDragAndDropUpdated(event);
}

int TabStripRegionView::OnDragUpdated(const ui::DropTargetEvent& event) {
  // This can be false because we can still receive drag events after
  // TabDragController is destroyed due to the asynchronous nature of the
  // platform DnD.
  if (TabDragController::IsSystemDragAndDropSessionRunning()) {
    TabDragController::OnSystemDragAndDropUpdated(event);
    return ui::DragDropTypes::DRAG_MOVE;
  }
  return ui::DragDropTypes::DRAG_NONE;
}

void TabStripRegionView::OnDragExited() {
  // See comment in OnDragUpdated().
  if (TabDragController::IsSystemDragAndDropSessionRunning()) {
    TabDragController::OnSystemDragAndDropExited();
  }
}

void TabStripRegionView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

gfx::Size TabStripRegionView::GetMinimumSize() const {
  gfx::Size tab_strip_min_size = tab_strip_->GetMinimumSize();
  // Cap the tabstrip minimum width to a reasonable value so browser windows
  // aren't forced to grow arbitrarily wide.
  const int max_min_width = 520;
  tab_strip_min_size.set_width(
      std::min(max_min_width, tab_strip_min_size.width()));
  return tab_strip_min_size;
}

views::View* TabStripRegionView::GetDefaultFocusableChild() {
  auto* focusable_child = tab_strip_->GetDefaultFocusableChild();
  return focusable_child ? focusable_child
                         : AccessiblePaneView::GetDefaultFocusableChild();
}

// static
void TabStripRegionView::ReportCaptionHitTestInReservedGrabHandleSpace(
    bool in_reserved_grab_handle_space) {
#if BUILDFLAG(IS_WIN)
  static bool button_down_previously = false;
  int primary_mouse_button =
      ::GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
  bool button_down_now =
      (::GetAsyncKeyState(primary_mouse_button) & 0x8000) != 0;
  if (button_down_now && !button_down_previously) {
    base::UmaHistogramBoolean(
        "Chrome.Frame.MouseDownCaptionHitTestInReservedGrabHandleSpace",
        in_reserved_grab_handle_space);
  }
  button_down_previously = button_down_now;
#endif
}

void TabStripRegionView::UpdateButtonBorders() {
  const int extra_vertical_space = GetLayoutConstant(TAB_STRIP_HEIGHT) -
                                   GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) -
                                   NewTabButton::kButtonSize.height();
  const int top_inset = extra_vertical_space / 2;
  const int bottom_inset = extra_vertical_space - top_inset +
                           GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
  // The new tab button is placed vertically exactly in the center of the
  // tabstrip. Extend the border of the button such that it extends to the top
  // of the tabstrip bounds. This is essential to ensure it is targetable on the
  // edge of the screen when in fullscreen mode and ensures the button abides
  // by the correct Fitt's Law behavior (https://crbug.com/1136557).
  // TODO(crbug.com/40727472): The left border is 0 in order to abut the NTB
  // directly with the tabstrip. That's the best immediately available
  // approximation to the prior behavior of aligning the NTB relative to the
  // trailing separator (instead of the right bound of the trailing tab). This
  // still isn't quite what we ideally want in the non-scrolling case, and
  // definitely isn't what we want in the scrolling case, so this naive approach
  // should be improved, likely by taking the scroll state of the tabstrip into
  // account.
  const auto border_insets = gfx::Insets::TLBR(top_inset, 0, bottom_inset, 0);
  if (new_tab_button_) {
    new_tab_button_->SetBorder(views::CreateEmptyBorder(border_insets));
  }
  if (tab_search_container_) {
    tab_search_container_->tab_search_button()->SetBorder(
        views::CreateEmptyBorder(border_insets));

    if (tab_search_container_->auto_tab_group_button()) {
      tab_search_container_->auto_tab_group_button()->SetBorder(
          views::CreateEmptyBorder(border_insets));
    }

    if (tab_search_container_->tab_declutter_button()) {
      tab_search_container_->tab_declutter_button()->SetBorder(
          views::CreateEmptyBorder(border_insets));
    }
  }
}

void TabStripRegionView::UpdateTabStripMargin() {
  // The new tab button overlaps the tabstrip. Render it to a layer and adjust
  // the tabstrip right margin to reserve space for it.
  std::optional<int> tab_strip_right_margin;
  if (new_tab_button_) {
    new_tab_button_->SetPaintToLayer();
    new_tab_button_->layer()->SetFillsBoundsOpaquely(false);
    // Inset between the tabstrip and new tab button should be reduced to
    // account for extra spacing.
    new_tab_button_->SetProperty(views::kViewIgnoredByLayoutKey, true);

    tab_strip_right_margin = new_tab_button_->GetPreferredSize().width() +
                             GetLayoutConstant(TAB_STRIP_PADDING);
  }

  // If the tab search button is before the tab strip, it also overlaps the
  // tabstrip, so give it the same treatment.
  std::optional<int> tab_strip_left_margin;
  if (tab_search_container_ && render_tab_search_before_tab_strip_) {
    // The `tab_search_container_` is being laid out manually.
    CHECK(tab_search_container_->GetProperty(views::kViewIgnoredByLayoutKey));

    // When tab search container shows before tab strip, add a margin to the
    // tab_strip_container_ to leave the correct amount of space for UI
    // components showing before tab strip. Currently the components are
    // `tab_search_container_` and `product_specifications_button` if it's
    // available.
    int product_specifications_button_width =
        product_specifications_button_
            ? product_specifications_button_->GetPreferredSize().width()
            : 0;
    tab_strip_left_margin = tab_search_container_->GetPreferredSize().width() +
                            product_specifications_button_width;

    // The TabSearchContainer should be 6 pixels from the left and the tabstrip
    // should have 6 px of padding between it and the tab_search button (not
    // including the corner radius).
    tab_strip_left_margin = tab_strip_left_margin.value() +
                            GetLayoutConstant(TAB_STRIP_PADDING) +
                            GetLayoutConstant(TAB_STRIP_PADDING) -
                            TabStyle::Get()->GetBottomCornerRadius();
  }

  UpdateButtonBorders();

  if (tab_strip_left_margin.has_value() || tab_strip_right_margin.has_value()) {
    tab_strip_container_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, tab_strip_left_margin.value_or(0), 0,
                          tab_strip_right_margin.value_or(0)));
  }
}

void TabStripRegionView::AdjustViewBoundsRect(View* view, int offset) {
  const gfx::Size view_size = view->GetPreferredSize();
  const int x =
      tab_strip_container_->x() + TabStyle::Get()->GetBottomCornerRadius() -
      GetLayoutConstant(TAB_STRIP_PADDING) - view_size.width() - offset;
  const gfx::Rect new_bounds = gfx::Rect(gfx::Point(x, 0), view_size);
  view->SetBoundsRect(new_bounds);
}

// Logger that periodically saves the tab search position. There should be 1
// instance per tabstrip.
class TabSearchPositionMetricsLogger {
 public:
  explicit TabSearchPositionMetricsLogger(
      const Profile* profile,
      base::TimeDelta logging_interval = base::Hours(1))
      : profile_(profile),
        logging_interval_(logging_interval),
        weak_ptr_factory_(this) {
    LogMetrics();
    ScheduleNextLog();
  }

  ~TabSearchPositionMetricsLogger() = default;

 private:
  // Logs the UMA metric for the tab search position.
  void LogMetrics() {
    base::UmaHistogramEnumeration(
        "Tabs.TabSearch.PositionInTabstrip",
        tabs::GetTabSearchTrailingTabstrip(profile_)
            ? TabStripRegionView::TabSearchPositionEnum::kTrailing
            : TabStripRegionView::TabSearchPositionEnum::kLeading);
  }

  // Sets up a task runner that calls back into the logging data.
  void ScheduleNextLog() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TabSearchPositionMetricsLogger::LogMetricAndReschedule,
                       weak_ptr_factory_.GetWeakPtr()),
        logging_interval_);
  }

  // Helper method for posting the task which logs and schedules the next log.
  void LogMetricAndReschedule() {
    LogMetrics();
    ScheduleNextLog();
  }

  // Profile for checking the pref value.
  const raw_ptr<const Profile> profile_;

  // Time in which this metric should be logged. Default is hourly.
  const base::TimeDelta logging_interval_;

  base::WeakPtrFactory<TabSearchPositionMetricsLogger> weak_ptr_factory_;
};

BEGIN_METADATA(TabStripRegionView)
END_METADATA
