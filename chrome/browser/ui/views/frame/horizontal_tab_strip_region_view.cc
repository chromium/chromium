// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tab_search_feature.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/border.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/interaction/view_subregion_anchor.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
constexpr int kTabStripRegionInternalPaddingMac = 12;
#endif

namespace {

class FrameGrabHandle : public views::View {
  METADATA_HEADER(FrameGrabHandle, views::View)

 public:
  FrameGrabHandle() {
    SetProperty(views::kElementIdentifierKey,
                kTabStripFrameGrabHandleElementId);
  }

  void Layout(PassKey) override {
    LayoutSuperclass<views::View>(this);

    int x = width() * 0.4;
    int y = height() * 0.7;
    dialog_anchor_->MaybeUpdateAnchor(gfx::Rect(x, y, 0, 0));
  }

  void AddedToWidget() override {
    dialog_anchor_ = std::make_unique<views::ViewSubregionAnchor>(
        kTabStripFrameDialogAnchorId, *this);
  }

  void RemovedFromWidget() override { dialog_anchor_.reset(); }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Reserve some space for the frame to be grabbed by, even if the tabstrip
    // is full.
    // TODO(tbergquist): Define this relative to the NTB insets again.
    return gfx::Size(42, 0);
  }

 private:
  // Anchor point for help bubbles and other dialogs that lies in an empty
  // region of the tabstrip.
  std::unique_ptr<views::ViewSubregionAnchor> dialog_anchor_;
};

BEGIN_METADATA(FrameGrabHandle)
END_METADATA

bool ShouldShowNewTabButton(BrowserWindowInterface* browser) {
  // `browser` can be null in tests and `app_controller` will be null if
  // the browser is not for an app.
  if (browser) {
    auto* const controller = web_app::AppBrowserController::From(browser);
    if (controller && controller->ShouldHideNewTabButton()) {
      return false;
    }
  }
  return true;
}

// Updates the border of `view` if the insets need to be updated.
void UpdateBorderInsetsIfNeeded(views::View* view,
                                const gfx::Insets& new_border_insets) {
  CHECK(view);
  if (!view->GetBorder() ||
      view->GetBorder()->GetInsets() != new_border_insets) {
    view->SetBorder(views::CreateEmptyBorder(new_border_insets));
  }
}

std::unique_ptr<TabStrip> CreateTabStrip(
    TabStripRegionView* tab_strip_region_view,
    BrowserView* browser_view) {
  std::unique_ptr<TabMenuModelFactory> tab_menu_model_factory;
  if (browser_view &&
      web_app::AppBrowserController::From(browser_view->browser())) {
    tab_menu_model_factory =
        web_app::AppBrowserController::From(browser_view->browser())
            ->GetTabMenuModelFactory();
  }

  auto tabstrip_controller = std::make_unique<BrowserTabStripController>(
      browser_view->browser()->GetTabStripModel(), browser_view,
      std::move(tab_menu_model_factory));

  std::unique_ptr<TabHoverCardController> hover_card_controller(
      std::make_unique<TabHoverCardController>(tab_strip_region_view,
                                               browser_view->browser()));
  auto tab_strip = std::make_unique<TabStrip>(std::move(tabstrip_controller),
                                              std::move(hover_card_controller));
  return tab_strip;
}

}  // namespace

// Logger that periodically saves the tab search position. There should be 1
// instance per tabstrip.
class TabSearchPositionMetricsLogger {
 public:
  explicit TabSearchPositionMetricsLogger(
      const BrowserWindowInterface* browser_window,
      base::TimeDelta logging_interval = base::Hours(1))
      : browser_window_(browser_window),
        logging_interval_(logging_interval),
        weak_ptr_factory_(this) {
    LogMetrics();
    ScheduleNextLog();
  }

  ~TabSearchPositionMetricsLogger() = default;

  void LogMetricsForTesting() { LogMetrics(); }

 private:
  // Logs the UMA metric for the tab search position.
  void LogMetrics() {
    const tabs::TabSearchPosition position =
        tabs::GetTabSearchPosition(browser_window_);
    if (position == tabs::TabSearchPosition::kLeadingHorizontalTabstrip ||
        position == tabs::TabSearchPosition::kTrailingHorizontalTabstrip) {
      base::UmaHistogramEnumeration(
          "Tabs.TabSearch.PositionInTabstrip2",
          position == tabs::TabSearchPosition::kTrailingHorizontalTabstrip
              ? HorizontalTabStripRegionView::TabSearchPositionEnum::kTrailing
              : HorizontalTabStripRegionView::TabSearchPositionEnum::kLeading);
    }
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

  // Browser window for checking the pref value.
  const raw_ptr<const BrowserWindowInterface> browser_window_;

  // Time in which this metric should be logged. Default is hourly.
  const base::TimeDelta logging_interval_;

  base::WeakPtrFactory<TabSearchPositionMetricsLogger> weak_ptr_factory_;
};

HorizontalTabStripRegionView::HorizontalTabStripRegionView(
    BrowserView* browser_view)
    : profile_(browser_view->GetProfile()),
      render_tab_search_before_tab_strip_(
          !tabs::GetDefaultTabSearchRightAligned() ||
          base::FeatureList::IsEnabled(tabs::kHorizontalTabStripComboButton)),
      tab_search_position_metrics_logger_(
          std::make_unique<TabSearchPositionMetricsLogger>(
              browser_view->browser())),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  views::SetCascadingColorProviderColor(
      this, views::kCascadingBackgroundColor,
      kColorTabBackgroundInactiveFrameInactive);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  BrowserWindowInterface* const browser = browser_view->browser();

  if (browser &&
      (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) &&
      base::FeatureList::IsEnabled(tabs::kHorizontalTabStripComboButton)) {
    combo_button_ = AddChildView(std::make_unique<TabStripComboButton>(
        browser, TabStripComboButton::Context::kHorizontalTabStrip));
    combo_button_->SetProperty(views::kCrossAxisAlignmentKey,
                               views::LayoutAlignment::kCenter);
    combo_button_->MaybeShowIPH();
  }

  if (base::FeatureList::IsEnabled(features::kTabGroupsFocusing)) {
    unfocus_button_ = AddChildView(std::make_unique<TabStripControlButton>(
        browser, views::Button::PressedCallback(),
        features::IsRoundedIconsEnabled() ? vector_icons::kArrowBackIcon
                                          : vector_icons::kArrowBackOldIcon,
        Edge::kNone, Edge::kNone));

    actions::ActionItem* const unfocus_action =
        actions::ActionManager::Get().FindAction(
            kActionUnfocusTabGroup, browser->GetActions()->root_action_item());
    CHECK(unfocus_action);
    action_view_controller_->CreateActionViewRelationship(
        unfocus_button_.get(), unfocus_action->GetAsWeakPtr());

    unfocus_button_->SetVisible(false);
    unfocus_button_->SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kCenter);
  }

  // Add and configure the TabSearchButton and TabStripComboButton.
  std::unique_ptr<TabSearchButton> tab_search_button;
  std::unique_ptr<TabStripActionContainer> tab_strip_action_container;
  if (browser &&
      (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL)) {
    // The Glic button visibility is dynamic and depends on profile state
    // (e.g., sign-in status, enterprise policies, recoverable errors).
    // We instantiate the action container if the profile is eligible (even if
    // the button is not currently shown, e.g. when signed out) so that it can
    // dynamically update its visibility when the profile state changes.
    if (glic::GlicEnabling::IsProfileEligible(profile_)) {
      tab_strip_action_container = std::make_unique<TabStripActionContainer>(
          browser, browser->GetFeatures().glic_nudge_controller());
      tab_strip_action_container->SetProperty(views::kCrossAxisAlignmentKey,
                                              views::LayoutAlignment::kStart);
    } else if (!base::FeatureList::IsEnabled(
                   tabs::kHorizontalTabStripComboButton)) {
      tab_search_button =
          std::make_unique<TabSearchButton>(browser, Edge::kNone, Edge::kNone);
      tab_search_button->SetProperty(views::kCrossAxisAlignmentKey,
                                     views::LayoutAlignment::kCenter);
    }
  }

  if (tab_search_button && render_tab_search_before_tab_strip_) {
    tab_search_button->SetPaintToLayer();
    tab_search_button->layer()->SetFillsBoundsOpaquely(false);

    tab_search_button_ = AddChildView(std::move(tab_search_button));

    // Inset between the tabsearch and tabstrip should be reduced to account for
    // extra spacing.
    tab_search_button_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  }

  tab_strip_ = AddChildView(CreateTabStrip(this, browser_view));

  // Allow the |tab_strip_| to grow into the free space available in
  // the HorizontalTabStripRegionView.
  const views::FlexSpecification tab_strip_flex_spec =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred);
  tab_strip_->SetProperty(views::kFlexBehaviorKey, tab_strip_flex_spec);

  if (ShouldShowNewTabButton(browser)) {
    std::unique_ptr<TabStripControlButton> tab_strip_control_button =
        std::make_unique<NewTabButton>(
            base::BindRepeating(&TabStrip::NewTabButtonPressed,
                                base::Unretained(tab_strip_)),
            features::IsRoundedIconsEnabled() ? vector_icons::kAddWeight500Icon
                                              : vector_icons::kAddOldIcon,
            Edge::kNone, Edge::kNone, browser);

    new_tab_button_ = AddChildView(std::move(tab_strip_control_button));

    new_tab_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));
    new_tab_button_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_NEWTAB));

#if BUILDFLAG(IS_LINUX)
    // On Linux, middle-clicking the New Tab Button triggers
    // paste and navigate, either to URLs or to search queries.
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

  if (browser && tab_search_button && !render_tab_search_before_tab_strip_) {
    tab_search_button_ = AddChildView(std::move(tab_search_button));
    tab_search_button_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0,
                          GetLayoutConstant(LayoutConstant::kTabStripPadding)));
  }
  if (tab_strip_action_container) {
    tab_strip_action_container_ =
        AddChildView(std::move(tab_strip_action_container));
  }
  UpdateTabStripMargin();
}
HorizontalTabStripRegionView::~HorizontalTabStripRegionView() {
  // These objects have pointers to TabStripController, which is also destoroyed
  // by this class. Remove child views that hold raw_ptr to TabStripController.
  if (tab_strip_action_container_) {
    RemoveChildViewT(std::exchange(tab_strip_action_container_, nullptr));
  }
  if (combo_button_) {
    RemoveChildViewT(std::exchange(combo_button_, nullptr));
  }
  if (new_tab_button_) {
    RemoveChildViewT(std::exchange(new_tab_button_, nullptr));
  }
  if (unfocus_button_) {
    RemoveChildViewT(std::exchange(unfocus_button_, nullptr));
  }
  if (tab_search_button_) {
    RemoveChildViewT(std::exchange(tab_search_button_, nullptr));
  }
}

bool HorizontalTabStripRegionView::IsPositionInWindowCaption(
    const gfx::Point& point) {
  if (new_tab_button_ && IsHitInView(new_tab_button_, point)) {
    return false;
  }

  if (combo_button_ && IsHitInView(combo_button_, point)) {
    return false;
  }

  if (render_tab_search_before_tab_strip_ && tab_search_button_ &&
      IsHitInView(tab_search_button_, point)) {
    return false;
  }

  // Perform a hit test against the |tab_strip_| to ensure that the
  // rect is within the visible portion of the |tab_strip_| before calling the
  // tab strip's |IsRectInWindowCaption()| for scrolling disabled. Defer to
  // scroll container if scrolling is enabled.
  // TODO(tluk): Address edge case where |rect| might partially intersect with
  // the |tab_strip_| and the |tab_strip_| but not over the same
  // pixels. This could lead to this returning false when it should be returning
  // true.
  if (IsHitInView(tab_strip_, point)) {
    gfx::RectF rect_in_target_coords_f(gfx::Rect(point, gfx::Size(1, 1)));
    View::ConvertRectToTarget(this, tab_strip_, &rect_in_target_coords_f);
    return tab_strip_->IsRectInWindowCaption(
        gfx::ToEnclosingRect(rect_in_target_coords_f));
  }

  // The child could have a non-rectangular shape, so if the rect is not in the
  // visual portions of the child view we treat it as a click to the caption.
  for (View* const child : children()) {
    if (child != tab_strip_ && child != reserved_grab_handle_space_ &&
        child->GetVisible() && IsHitInView(child, point)) {
      return false;
    }
  }

  return true;
}

views::View::Views HorizontalTabStripRegionView::GetChildrenInZOrder() {
  views::View::Views children;

  if (tab_strip_) {
    children.emplace_back(tab_strip_.get());
  }

  if (new_tab_button_) {
    children.emplace_back(new_tab_button_.get());
  }

  if (combo_button_) {
    children.emplace_back(combo_button_.get());
  }

  if (unfocus_button_) {
    children.emplace_back(unfocus_button_.get());
  }

  if (tab_search_button_) {
    children.emplace_back(tab_search_button_.get());
  }

  if (tab_strip_action_container_) {
    children.emplace_back(tab_strip_action_container_.get());
  }

  if (reserved_grab_handle_space_) {
    children.emplace_back(reserved_grab_handle_space_.get());
  }

  return children;
}

// The TabSearchButton need bounds that overlap the TabStripContainer, which
// FlexLayout doesn't currently support. Because of this the TSB bounds are
// manually calculated.
void HorizontalTabStripRegionView::Layout(PassKey) {
  if (!tab_strip_set_) {
    return;
  }

  const bool tab_search_button_before_tab_strip =
      tab_search_button_ && render_tab_search_before_tab_strip_;
  if (tab_search_button_before_tab_strip ||
      (unfocus_button_ && unfocus_button_->GetVisible()) || combo_button_) {
    UpdateTabStripMargin();
  }

  LayoutSuperclass<views::AccessiblePaneView>(this);

  int leading_offset = 0;
  if (tab_search_button_before_tab_strip) {
    AdjustViewBoundsRect(tab_search_button_, leading_offset);
    leading_offset += tab_search_button_->GetPreferredSize().width() +
                      GetLayoutConstant(LayoutConstant::kTabStripPadding);
  }

  if (unfocus_button_ && unfocus_button_->GetVisible()) {
    AdjustViewBoundsRect(unfocus_button_, leading_offset);
    leading_offset += unfocus_button_->GetPreferredSize().width() +
                      GetLayoutConstant(LayoutConstant::kTabStripPadding);
  }

  if (combo_button_) {
    AdjustViewBoundsRect(combo_button_, leading_offset);
  }

  views::View* button_to_paint_to_layer = new_tab_button_;

  if (button_to_paint_to_layer) {
    // The button needs to be layered on top of the tabstrip to achieve
    // negative margins.
    gfx::Size button_size = button_to_paint_to_layer->GetPreferredSize();

    // The y position is measured from the bottom of the tabstrip, and then
    // padding and button height are removed.
    int x = tab_strip_->bounds().right() -
            TabStyle::Get()->GetBottomCornerRadius() +
            GetLayoutConstant(LayoutConstant::kTabStripPadding) +
            GetLayoutConstant(LayoutConstant::kNewTabButtonLeadingMargin);

    gfx::Point button_new_position = gfx::Point(x, GetInsets().top());
    gfx::Rect button_new_bounds = gfx::Rect(button_new_position, button_size);

    // If the tabsearch button is before the tabstrip container, then manually
    // set the bounds.
    button_to_paint_to_layer->SetBoundsRect(button_new_bounds);
  }
}

bool HorizontalTabStripRegionView::CanDrop(const OSExchangeData& data) {
  return TabDragController::IsSystemDnDSessionRunning() &&
         data.HasCustomFormat(ui::ClipboardFormatType::CustomPlatformType(
             ui::kMimeTypeWindowDrag));
}

bool HorizontalTabStripRegionView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(
      ui::ClipboardFormatType::CustomPlatformType(ui::kMimeTypeWindowDrag));
  return true;
}

void HorizontalTabStripRegionView::OnDragEntered(
    const ui::DropTargetEvent& event) {
  CHECK(TabDragController::IsSystemDnDSessionRunning());
  TabDragController::OnSystemDnDUpdated(event);
}

int HorizontalTabStripRegionView::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  // This can be false because we can still receive drag events after
  // TabDragController is destroyed due to the asynchronous nature of the
  // platform DnD.
  if (TabDragController::IsSystemDnDSessionRunning()) {
    TabDragController::OnSystemDnDUpdated(event);
    return ui::DragDropTypes::DRAG_MOVE;
  }
  return ui::DragDropTypes::DRAG_NONE;
}

void HorizontalTabStripRegionView::OnDragExited() {
  // See comment in OnDragUpdated().
  if (TabDragController::IsSystemDnDSessionRunning()) {
    TabDragController::OnSystemDnDExited();
  }
}

void HorizontalTabStripRegionView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

gfx::Size HorizontalTabStripRegionView::GetMinimumSize() const {
  gfx::Size tab_strip_min_size = tab_strip_->GetMinimumSize();
  // Cap the tabstrip minimum width to a reasonable value so browser windows
  // aren't forced to grow arbitrarily wide.
  const int max_min_width = 520;
  tab_strip_min_size.set_width(
      std::min(max_min_width, tab_strip_min_size.width()));
  return tab_strip_min_size;
}

gfx::Size HorizontalTabStripRegionView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetLayoutManager()->GetPreferredSize(this, available_size);
}

views::View* HorizontalTabStripRegionView::GetDefaultFocusableChild() {
  auto* focusable_child = tab_strip_->GetDefaultFocusableChild();
  return focusable_child ? focusable_child
                         : AccessiblePaneView::GetDefaultFocusableChild();
}

views::Button* HorizontalTabStripRegionView::GetTabSearchButton() {
  if (combo_button_) {
    return combo_button_->end_button();
  }
  return tab_search_button_;
}

views::LabelButton* HorizontalTabStripRegionView::GetGlicButton() {
  return tab_strip_action_container_
             ? tab_strip_action_container_->GetGlicButton()
             : nullptr;
}

void HorizontalTabStripRegionView::InitializeTabStrip() {
  if (tab_strip_set_) {
    return;
  }

  tab_strip_->Initialize();
  static_cast<BrowserTabStripController*>(tab_strip_->controller())
      ->InitFromModel(tab_strip_);
  tab_strip_set_ = true;
}

void HorizontalTabStripRegionView::ResetTabStrip() {
  tab_strip_set_ = false;
  static_cast<BrowserTabStripController*>(tab_strip_->controller())->Reset();
  tab_strip_->Reset();
}

bool HorizontalTabStripRegionView::IsTabStripEditable() const {
  return tab_strip_->IsTabStripEditable();
}

void HorizontalTabStripRegionView::DisableTabStripEditingForTesting() {
  tab_strip_->DisableTabStripEditingForTesting();  // IN-TEST
}

bool HorizontalTabStripRegionView::IsTabStripCloseable() const {
  return tab_strip_->IsTabStripCloseable();
}

void HorizontalTabStripRegionView::UpdateLoadingAnimations(
    const base::TimeDelta& elapsed_time) {
  tab_strip_->UpdateLoadingAnimations(elapsed_time);
}

std::optional<int> HorizontalTabStripRegionView::GetFocusedTabIndex() const {
  for (int i = 0; i < tab_strip_->GetTabCount(); ++i) {
    if (tab_strip_->tab_at(i)->HasFocus()) {
      return i;
    }
  }
  return std::nullopt;
}

const tabs::TabData& HorizontalTabStripRegionView::GetTabData(
    const tabs::TabHandle& tab) {
  for (int i = 0; i < tab_strip_->GetTabCount(); ++i) {
    Tab* tab_view = tab_strip_->tab_at(i);
    if (tab_view->tab_handle() == tab) {
      return tab_view->data();
    }
  }

  NOTREACHED() << "Tab view not found for handle";
}

views::View* HorizontalTabStripRegionView::GetTabAnchorViewAt(int tab_index) {
  return tab_strip_->tab_at(tab_index);
}

views::View* HorizontalTabStripRegionView::GetTabGroupAnchorView(
    const tab_groups::TabGroupId& group) {
  return tab_strip_->group_header(group);
}

void HorizontalTabStripRegionView::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  CHECK(unfocus_button_);
  unfocus_button_->SetVisible(new_focused_group_id.has_value());
  if (old_focused_group_id.has_value() != new_focused_group_id.has_value()) {
    UpdateTabStripMargin();
  }
  tab_strip_->OnTabGroupFocusChanged(new_focused_group_id,
                                     old_focused_group_id);
  InvalidateLayout();
}

TabDragContext* HorizontalTabStripRegionView::GetDragContext() {
  return tab_strip_->GetDragContext();
}

std::optional<BrowserRootView::DropIndex>
HorizontalTabStripRegionView::GetDropIndex(const ui::DropTargetEvent& event) {
  return tab_strip_->GetDropIndex(event);
}

BrowserRootView::DropTarget* HorizontalTabStripRegionView::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  ConvertPointToTarget(this, tab_strip_, &loc_in_local_coords);
  return tab_strip_->GetDropTarget(loc_in_local_coords);
}

views::View* HorizontalTabStripRegionView::GetViewForDrop() {
  return tab_strip_;
}

void HorizontalTabStripRegionView::SetTabStripObserver(
    TabStripObserver* observer) {
  tab_strip_->SetTabStripObserver(observer);
}

views::View* HorizontalTabStripRegionView::GetTabStripView() {
  return tab_strip_;
}

std::unique_ptr<ExpandOnHoverLock>
HorizontalTabStripRegionView::GetExpandOnHoverLock(
    ExpandOnHoverLockType lock_type) {
  return nullptr;
}

bool HorizontalTabStripRegionView::HasLeadingButtons() const {
  if (combo_button_ && combo_button_->GetVisible() &&
      ((combo_button_->start_button() &&
        combo_button_->start_button()->GetVisible()) ||
       (combo_button_->end_button() &&
        combo_button_->end_button()->GetVisible()))) {
    return true;
  }
  if (unfocus_button_ && unfocus_button_->GetVisible()) {
    return true;
  }
  if (tab_search_button_ && render_tab_search_before_tab_strip_ &&
      tab_search_button_->GetVisible()) {
    return true;
  }
  return false;
}

void HorizontalTabStripRegionView::LogTabSearchPositionForTesting() {
  tab_search_position_metrics_logger_->LogMetricsForTesting();  // IN-TEST
}

void HorizontalTabStripRegionView::UpdateButtonBorders() {
  const int extra_vertical_space =
      GetLayoutConstant(LayoutConstant::kTabStripHeight) -
      GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap) -
      NewTabButton::kButtonSize.height();
  const int top_inset = extra_vertical_space / 2;
  const int bottom_inset =
      extra_vertical_space - top_inset +
      GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap);
  // The new tab button is placed vertically exactly in the center of the
  // tabstrip. Extend the border of the button such that it extends to the top
  // of the tabstrip bounds. This is essential to ensure it is targetable on the
  // edge of the screen when in fullscreen mode and ensures the button abides
  // by the correct Fitt's Law behavior (https://crbug.com/40152330).
  // TODO(crbug.com/40727472): The left border is 0 in order to abut the NTB
  // directly with the tabstrip. That's the best immediately available
  // approximation to the prior behavior of aligning the NTB relative to the
  // trailing separator (instead of the right bound of the trailing tab). This
  // still isn't quite what we ideally want in the non-scrolling case, and
  // definitely isn't what we want in the scrolling case, so this naive approach
  // should be improved, likely by taking the scroll state of the tabstrip into
  // account.
  const auto border_insets = gfx::Insets::TLBR(top_inset, 0, bottom_inset, 0);
  if (tab_strip_action_container_) {
    tab_strip_action_container_->UpdateButtonBorders(border_insets);
  }
  if (combo_button_) {
    UpdateBorderInsetsIfNeeded(combo_button_, border_insets);
  }
  if (new_tab_button_) {
    UpdateBorderInsetsIfNeeded(new_tab_button_, border_insets);
  }
  if (unfocus_button_) {
    UpdateBorderInsetsIfNeeded(unfocus_button_, border_insets);
  }
  if (tab_search_button_) {
    UpdateBorderInsetsIfNeeded(tab_search_button_, border_insets);
  }
}

void HorizontalTabStripRegionView::UpdateTabStripMargin() {
#if BUILDFLAG(IS_MAC)
  if (HasLeadingButtons()) {
    // When leading buttons are present, maintain a consistent 12px gap from
    // the caption buttons on Mac.
    SetProperty(views::kInternalPaddingKey,
                gfx::Insets::TLBR(0, kTabStripRegionInternalPaddingMac, 0, 0));
  } else {
    ClearProperty(views::kInternalPaddingKey);
  }
#endif

  // The new tab button overlaps the tabstrip. Render it to a layer and adjust
  // the tabstrip right margin to reserve space for it.
  std::optional<int> tab_strip_right_margin;
  views::View* button_to_paint_to_layer = new_tab_button_;

  if (button_to_paint_to_layer) {
    button_to_paint_to_layer->SetPaintToLayer();
    button_to_paint_to_layer->layer()->SetFillsBoundsOpaquely(false);
    // Inset between the tabstrip and new tab button should be reduced to
    // account for extra spacing.
    button_to_paint_to_layer->SetProperty(views::kViewIgnoredByLayoutKey, true);

    tab_strip_right_margin =
        button_to_paint_to_layer->GetPreferredSize().width() +
        GetLayoutConstant(LayoutConstant::kTabStripPadding);
  }

  // If the tab search button is before the tab strip, it also overlaps the
  // tabstrip, so give it the same treatment.
  std::optional<int> tab_strip_left_margin;
  int current_leading_width = 0;

  if (tab_search_button_ && render_tab_search_before_tab_strip_) {
    // The `tab_search_button_` is being laid out manually.
    CHECK(tab_search_button_->GetProperty(views::kViewIgnoredByLayoutKey));
    current_leading_width +=
        tab_search_button_->GetPreferredSize().width() +
        GetLayoutConstant(LayoutConstant::kTabStripPadding);
  }

  if (unfocus_button_ && unfocus_button_->GetVisible()) {
    unfocus_button_->SetPaintToLayer();
    unfocus_button_->layer()->SetFillsBoundsOpaquely(false);
    unfocus_button_->SetProperty(views::kViewIgnoredByLayoutKey, true);
    current_leading_width +=
        unfocus_button_->GetPreferredSize().width() +
        GetLayoutConstant(LayoutConstant::kTabStripPadding);
  }

  if (combo_button_) {
    combo_button_->SetPaintToLayer();
    combo_button_->layer()->SetFillsBoundsOpaquely(false);
    combo_button_->SetProperty(views::kViewIgnoredByLayoutKey, true);
    current_leading_width +=
        combo_button_->GetPreferredSize().width() +
        GetLayoutConstant(LayoutConstant::kTabStripPadding);
  }

  if (current_leading_width > 0) {
    tab_strip_left_margin =
        current_leading_width +
        GetLayoutConstant(LayoutConstant::kTabStripPadding) -
        TabStyle::Get()->GetBottomCornerRadius();
  }

  UpdateButtonBorders();

  if (tab_strip_left_margin.has_value() || tab_strip_right_margin.has_value()) {
    tab_strip_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, tab_strip_left_margin.value_or(0), 0,
                          tab_strip_right_margin.value_or(0)));
  }
}

void HorizontalTabStripRegionView::AdjustViewBoundsRect(View* view,
                                                        int offset) {
  const gfx::Size view_size = view->GetPreferredSize();
  const int x = tab_strip_->x() + TabStyle::Get()->GetBottomCornerRadius() -
                GetLayoutConstant(LayoutConstant::kTabStripPadding) -
                view_size.width() - offset;
  const gfx::Rect new_bounds =
      gfx::Rect(gfx::Point(x, GetInsets().top()), view_size);
  view->SetBoundsRect(new_bounds);
}

BEGIN_METADATA(HorizontalTabStripRegionView)
END_METADATA
