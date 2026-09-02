// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/geic/geic_enabling.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/shared/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/border.h"
#include "ui/views/cascading_property.h"
#include "ui/views/interaction/view_subregion_anchor.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
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
  auto tabstrip_controller = std::make_unique<BrowserTabStripController>(
      browser_view->browser()->GetTabStripModel(), browser_view);

  std::unique_ptr<TabHoverCardController> hover_card_controller(
      std::make_unique<TabHoverCardController>(tab_strip_region_view,
                                               browser_view->browser()));
  auto tab_strip = std::make_unique<TabStrip>(std::move(tabstrip_controller),
                                              std::move(hover_card_controller));
  return tab_strip;
}

}  // namespace

HorizontalTabStripRegionViewOld::HorizontalTabStripRegionViewOld(
    BrowserView* browser_view)
    : browser_view_(browser_view),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  views::SetCascadingColorProviderColor(
      this, views::kCascadingBackgroundColor,
      kColorTabBackgroundInactiveFrameInactive);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  BrowserWindowInterface* const browser = browser_view->browser();

  if (browser &&
      (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL)) {
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
            kActionUnfocusTabGroup,
            BrowserActions::From(browser)->root_action_item());
    CHECK(unfocus_action);
    action_view_controller_->CreateActionViewRelationship(
        unfocus_button_.get(), unfocus_action->GetAsWeakPtr());

    unfocus_button_subscription_ =
        unfocus_button_->AddVisibleChangedCallback(base::BindRepeating(
            &HorizontalTabStripRegionViewOld::OnUnfocusButtonVisibilityChanged,
            base::Unretained(this)));

    unfocus_button_->SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kCenter);
  }

  // Add and configure the TabStripComboButton.
  std::unique_ptr<TabStripActionContainer> tab_strip_action_container;
  if (browser &&
      (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL)) {
    // The Glic button visibility is dynamic and depends on profile state
    // (e.g., sign-in status, enterprise policies, recoverable errors).
    // We instantiate the action container if the profile is eligible (even if
    // the button is not currently shown, e.g. when signed out) so that it can
    // dynamically update its visibility when the profile state changes.
    if (geic::IsGeicEnabled(browser_view->GetProfile()) ||
        glic::GlicEnabling::IsProfileEligible(browser_view->GetProfile())) {
      tab_strip_action_container =
          std::make_unique<TabStripActionContainer>(browser);
      tab_strip_action_container->SetProperty(views::kCrossAxisAlignmentKey,
                                              views::LayoutAlignment::kStart);
    }
  }

  tab_strip_ = AddChildView(CreateTabStrip(this, browser_view));

  // Allow the |tab_strip_| to grow into the free space available in
  // the HorizontalTabStripRegionViewOld.
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
            features::IsRoundedIconsEnabled()
                ? vector_icons::kAddWeight500CustomIcon
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

  if (tab_strip_action_container) {
    tab_strip_action_container_ =
        AddChildView(std::move(tab_strip_action_container));
  }
  UpdateTabStripMargin();
}

HorizontalTabStripRegionViewOld::~HorizontalTabStripRegionViewOld() {
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
}

bool HorizontalTabStripRegionViewOld::IsPositionInWindowCaption(
    const gfx::Point& point) {
  if (new_tab_button_ && IsHitInView(new_tab_button_, point)) {
    return false;
  }

  if (combo_button_ && IsHitInView(combo_button_, point)) {
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

views::View::Views HorizontalTabStripRegionViewOld::GetChildrenInZOrder() {
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
void HorizontalTabStripRegionViewOld::Layout(PassKey) {
  if (!tab_strip_set_) {
    return;
  }

  UpdateTabStripMargin();
  LayoutSuperclass<views::AccessiblePaneView>(this);

  int leading_offset = 0;
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

bool HorizontalTabStripRegionViewOld::CanDrop(const OSExchangeData& data) {
  return TabDragController::IsSystemDnDSessionRunning() &&
         data.HasCustomFormat(ui::ClipboardFormatType::CustomPlatformType(
             ui::kMimeTypeWindowDrag));
}

bool HorizontalTabStripRegionViewOld::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(
      ui::ClipboardFormatType::CustomPlatformType(ui::kMimeTypeWindowDrag));
  return true;
}

void HorizontalTabStripRegionViewOld::OnDragEntered(
    const ui::DropTargetEvent& event) {
  CHECK(TabDragController::IsSystemDnDSessionRunning());
  TabDragController::OnSystemDnDUpdated(event);
}

int HorizontalTabStripRegionViewOld::OnDragUpdated(
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

void HorizontalTabStripRegionViewOld::OnDragExited() {
  // See comment in OnDragUpdated().
  if (TabDragController::IsSystemDnDSessionRunning()) {
    TabDragController::OnSystemDnDExited();
  }
}

void HorizontalTabStripRegionViewOld::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

gfx::Size HorizontalTabStripRegionViewOld::GetMinimumSize() const {
  gfx::Size tab_strip_min_size = tab_strip_->GetMinimumSize();
  // Cap the tabstrip minimum width to a reasonable value so browser windows
  // aren't forced to grow arbitrarily wide.
  const int max_min_width = 520;
  tab_strip_min_size.set_width(
      std::min(max_min_width, tab_strip_min_size.width()));
  return tab_strip_min_size;
}

gfx::Size HorizontalTabStripRegionViewOld::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetLayoutManager()->GetPreferredSize(this, available_size);
}

views::View* HorizontalTabStripRegionViewOld::GetDefaultFocusableChild() {
  auto* focusable_child = tab_strip_->GetDefaultFocusableChild();
  return focusable_child ? focusable_child
                         : AccessiblePaneView::GetDefaultFocusableChild();
}

Profile* HorizontalTabStripRegionViewOld::profile() {
  return browser_view_->GetProfile();
}

void HorizontalTabStripRegionViewOld::InitializeTabStrip() {
  if (tab_strip_set_) {
    return;
  }

  tab_strip_->Initialize();
  static_cast<BrowserTabStripController*>(tab_strip_->controller())
      ->InitFromModel(tab_strip_);
  tab_strip_set_ = true;
}

void HorizontalTabStripRegionViewOld::ResetTabStrip() {
  tab_strip_set_ = false;
  static_cast<BrowserTabStripController*>(tab_strip_->controller())->Reset();
  tab_strip_->Reset();
}

bool HorizontalTabStripRegionViewOld::IsTabStripEditable() const {
  return tab_strip_->IsTabStripEditable();
}

void HorizontalTabStripRegionViewOld::DisableTabStripEditingForTesting() {
  tab_strip_->DisableTabStripEditingForTesting();  // IN-TEST
}

bool HorizontalTabStripRegionViewOld::IsTabStripCloseable() const {
  return tab_strip_->IsTabStripCloseable();
}

void HorizontalTabStripRegionViewOld::UpdateLoadingAnimations(
    const base::TimeDelta& elapsed_time) {
  tab_strip_->UpdateLoadingAnimations(elapsed_time);
}

std::optional<int> HorizontalTabStripRegionViewOld::GetFocusedTabIndex() const {
  for (int i = 0; i < tab_strip_->GetTabCount(); ++i) {
    if (tab_strip_->tab_at(i)->HasFocus()) {
      return i;
    }
  }
  return std::nullopt;
}

const tabs::TabData& HorizontalTabStripRegionViewOld::GetTabData(
    const tabs::TabHandle& tab) {
  for (int i = 0; i < tab_strip_->GetTabCount(); ++i) {
    Tab* tab_view = tab_strip_->tab_at(i);
    if (tab_view->tab_handle() == tab) {
      return tab_view->data();
    }
  }
  NOTREACHED() << "Tab view not found for handle";
}

views::View* HorizontalTabStripRegionViewOld::GetTabAnchorView(
    const tabs::TabHandle& tab) {
  for (int i = 0; i < tab_strip_->GetTabCount(); ++i) {
    Tab* tab_view = tab_strip_->tab_at(i);
    if (tab_view->tab_handle() == tab) {
      return tab_view;
    }
  }
  return nullptr;
}

views::View* HorizontalTabStripRegionViewOld::GetTabGroupAnchorView(
    const tab_groups::TabGroupId& group) {
  return tab_strip_->group_header(group);
}

void HorizontalTabStripRegionViewOld::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  tab_strip_->OnTabGroupFocusChanged(new_focused_group_id,
                                     old_focused_group_id);
}

void HorizontalTabStripRegionViewOld::OnUnfocusButtonVisibilityChanged() {
  UpdateTabStripMargin();
  InvalidateLayout();
}

TabDragContext* HorizontalTabStripRegionViewOld::GetDragContext() {
  return tab_strip_->GetDragContext();
}

TabDragTarget* HorizontalTabStripRegionViewOld::GetTabDragTarget(
    const gfx::Point& point_in_screen) {
  // This is not used for HorizontalTabStripRegionViewOld.
  return nullptr;
}

std::optional<BrowserRootView::DropIndex>
HorizontalTabStripRegionViewOld::GetDropIndex(
    const ui::DropTargetEvent& event) {
  return tab_strip_->GetDropIndex(event);
}

BrowserRootView::DropTarget* HorizontalTabStripRegionViewOld::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  ConvertPointToTarget(this, tab_strip_, &loc_in_local_coords);
  return tab_strip_->GetDropTarget(loc_in_local_coords);
}

views::View* HorizontalTabStripRegionViewOld::GetViewForDrop() {
  return tab_strip_;
}

void HorizontalTabStripRegionViewOld::SetTabStripObserver(
    TabStripObserver* observer) {
  tab_strip_->SetTabStripObserver(observer);
}

views::View* HorizontalTabStripRegionViewOld::GetTabStripView() {
  return tab_strip_;
}

TabHoverCardController*
HorizontalTabStripRegionViewOld::GetHoverCardController() {
  return tab_strip_ ? tab_strip_->hover_card_controller() : nullptr;
}

std::unique_ptr<ExpandOnHoverLock>
HorizontalTabStripRegionViewOld::GetExpandOnHoverLock(
    ExpandOnHoverLockType lock_type) {
  return nullptr;
}

bool HorizontalTabStripRegionViewOld::HasLeadingButtons() const {
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
  return false;
}

void HorizontalTabStripRegionViewOld::UpdateButtonBorders() {
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
}

void HorizontalTabStripRegionViewOld::UpdateTabStripMargin() {
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

  if (unfocus_button_ && unfocus_button_->GetVisible()) {
    unfocus_button_->SetPaintToLayer();
    unfocus_button_->layer()->SetFillsBoundsOpaquely(false);
    unfocus_button_->SetProperty(views::kViewIgnoredByLayoutKey, true);
    current_leading_width +=
        unfocus_button_->GetPreferredSize().width() +
        GetLayoutConstant(LayoutConstant::kTabStripPadding);
  }

  if (combo_button_ && ((combo_button_->start_button() &&
                         combo_button_->start_button()->GetVisible()) ||
                        (combo_button_->end_button() &&
                         combo_button_->end_button()->GetVisible()))) {
    combo_button_->SetPaintToLayer();
    combo_button_->layer()->SetFillsBoundsOpaquely(false);
    combo_button_->SetProperty(views::kViewIgnoredByLayoutKey, true);
    current_leading_width +=
        combo_button_->GetPreferredSize().width() +
        GetLayoutConstant(LayoutConstant::kTabStripPadding);
  }

  if (current_leading_width > 0) {
    tab_strip_left_margin = current_leading_width +
                            GetLayoutConstant(LayoutConstant::kTabStripPadding);
  }

  bool subtract_radius = current_leading_width > 0;
#if BUILDFLAG(IS_MAC)
  const ImmersiveModeController* const immersive_mode_controller =
      browser_view_->browser()
          ? ImmersiveModeController::From(browser_view_->browser())
          : nullptr;
  const bool is_immersive_mode_enabled =
      immersive_mode_controller && immersive_mode_controller->IsEnabled();
  if (is_immersive_mode_enabled) {
    subtract_radius = false;
  }
#endif

  if (subtract_radius) {
    tab_strip_left_margin.value() -= TabStyle::Get()->GetBottomCornerRadius();
  }

  UpdateButtonBorders();

  if (tab_strip_left_margin.has_value() || tab_strip_right_margin.has_value()) {
    tab_strip_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, tab_strip_left_margin.value_or(0), 0,
                          tab_strip_right_margin.value_or(0)));
  }
}

void HorizontalTabStripRegionViewOld::AdjustViewBoundsRect(View* view,
                                                           int offset) {
  const gfx::Size view_size = view->GetPreferredSize();
  const int x = tab_strip_->x() + TabStyle::Get()->GetBottomCornerRadius() -
                GetLayoutConstant(LayoutConstant::kTabStripPadding) -
                view_size.width() - offset;
  const gfx::Rect new_bounds =
      gfx::Rect(gfx::Point(x, GetInsets().top()), view_size);
  view->SetBoundsRect(new_bounds);
}

void HorizontalTabStripRegionViewOld::OnGlassFrameEligibilityChanged(
    bool is_eligible) {
  tab_strip_->SetIsGlassFrame(is_eligible);
  SchedulePaint();
}

BEGIN_METADATA(HorizontalTabStripRegionViewOld)
END_METADATA

HorizontalTabStripRegionViewNew::HorizontalTabStripRegionViewNew(
    BrowserView* browser_view)
    : BaseTabStripRegionView(
          browser_view,
          BrowserActions::From(browser_view->browser())->root_action_item(),
          TabStripOrientation::kHorizontal),
      action_view_controller_(std::make_unique<views::ActionViewController>()),
      subscription_(
          ui::TouchUiController::Get()->RegisterCallback(base::BindRepeating(
              &HorizontalTabStripRegionViewNew::UpdateButtonBorders,
              base::Unretained(this)))) {
  views::SetCascadingColorProviderColor(
      this, views::kCascadingBackgroundColor,
      kColorTabBackgroundInactiveFrameInactive);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  BrowserWindowInterface* const browser = browser_view->browser();

  std::unique_ptr<TabStripActionContainer> tab_strip_action_container;
  if (browser &&
      (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL)) {
    combo_button_ = AddChildView(std::make_unique<TabStripComboButton>(
        browser, TabStripComboButton::Context::kHorizontalTabStrip));
    combo_button_->SetProperty(views::kCrossAxisAlignmentKey,
                               views::LayoutAlignment::kCenter);

    if (glic::GlicEnabling::IsProfileEligible(browser_view->GetProfile())) {
      tab_strip_action_container =
          std::make_unique<TabStripActionContainer>(browser);
      tab_strip_action_container->SetProperty(views::kCrossAxisAlignmentKey,
                                              views::LayoutAlignment::kStart);
    }
  }

  if (browser && ShouldShowNewTabButton(browser)) {
    auto new_tab_button = std::make_unique<shared::NewTabButton>(
        browser, TabStripControlButton::kButtonSize.width(),
        TabStripControlButton::kIconSize,
        TabStripControlButton::kButtonSize.width() / 2.0f);
    new_tab_button->SetPaintTransparentForGlass(true);
    new_tab_button_ = AddChildView(std::move(new_tab_button));
    new_tab_button_->SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kCenter);
  }

  reserved_grab_handle_space_ =
      AddChildView(std::make_unique<FrameGrabHandle>());
  reserved_grab_handle_space_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(3));

  SetProperty(views::kElementIdentifierKey, kTabStripRegionElementId);

  if (tab_strip_action_container) {
    tab_strip_action_container_ =
        AddChildView(std::move(tab_strip_action_container));
  }

  UpdateButtonBorders();
}

HorizontalTabStripRegionViewNew::~HorizontalTabStripRegionViewNew() {
  if (tab_strip_action_container_) {
    RemoveChildViewT(std::exchange(tab_strip_action_container_, nullptr));
  }
  if (combo_button_) {
    RemoveChildViewT(std::exchange(combo_button_, nullptr));
  }
  if (new_tab_button_) {
    RemoveChildViewT(std::exchange(new_tab_button_, nullptr));
  }
}

bool HorizontalTabStripRegionViewNew::IsPositionInWindowCaption(
    const gfx::Point& point) {
  if (new_tab_button_ && IsHitInView(new_tab_button_, point)) {
    return false;
  }
  if (combo_button_ && IsHitInView(combo_button_, point)) {
    return false;
  }
  if (tab_strip_view() && IsHitInView(tab_strip_view(), point)) {
    gfx::Point point_in_tab_strip = point;
    views::View::ConvertPointToTarget(this, tab_strip_view(),
                                      &point_in_tab_strip);
    if (!tab_strip_view()->IsPositionInWindowCaption(point_in_tab_strip)) {
      return false;
    }
  }
  return true;
}

views::View::Views HorizontalTabStripRegionViewNew::GetChildrenInZOrder() {
  views::View::Views children;
  if (tab_strip_view()) {
    children.emplace_back(tab_strip_view());
  }
  if (GetDragContext()) {
    children.emplace_back(GetDragContext());
  }
  if (new_tab_button_) {
    children.emplace_back(new_tab_button_.get());
  }
  if (combo_button_) {
    children.emplace_back(combo_button_.get());
  }
  if (reserved_grab_handle_space_) {
    children.emplace_back(reserved_grab_handle_space_.get());
  }
  if (tab_strip_action_container_) {
    children.emplace_back(tab_strip_action_container_.get());
  }
  return children;
}

void HorizontalTabStripRegionViewNew::Layout(PassKey) {
  LayoutSuperclass<BaseTabStripRegionView>(this);
}

gfx::Size HorizontalTabStripRegionViewNew::GetMinimumSize() const {
  if (tab_strip_view()) {
    gfx::Size tab_strip_min_size = tab_strip_view()->GetMinimumSize();
    // Cap the tabstrip minimum width to a reasonable value so browser windows
    // aren't forced to grow arbitrarily wide.
    const int max_min_width = 520;
    tab_strip_min_size.set_width(
        std::min(max_min_width, tab_strip_min_size.width()));
    return tab_strip_min_size;
  }
  return gfx::Size();
}

gfx::Size HorizontalTabStripRegionViewNew::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (tab_strip_view()) {
    return tab_strip_view()->GetPreferredSize(available_size);
  }
  return gfx::Size();
}

views::View* HorizontalTabStripRegionViewNew::GetTabStripView() {
  return tab_strip_view();
}

gfx::Rect HorizontalTabStripRegionViewNew::GetTabStripDraggableBounds() const {
  if (tab_strip_view()) {
    return tab_strip_view()->GetBoundsInScreen();
  }
  return gfx::Rect();
}

gfx::Point HorizontalTabStripRegionViewNew::GetLinkDropArrowPosition(
    const BrowserRootView::DropIndex& drop_index,
    DropArrow::Direction* direction) {
  // By default, have the arrow point down towards the tab strip.
  *direction = DropArrow::Direction::kDown;

  if (tab_strip_model()->count() == 0) {
    return GetBoundsInScreen().origin();
  }

  const int overlap = TabStyle::Get()->GetTabOverlap();
  const bool is_rtl = base::i18n::IsRTL();
  const bool replace_index =
      drop_index.relative_to_index ==
      BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex;

  // Calculates the X coordinate for the drop arrow at a view's edge,
  // factoring in RTL and tab overlap. `is_after` indicates whether
  // the drop is placed after the provided bounds (true) or before them (false).
  auto GetAdjustedXForDrop = [&](const gfx::Rect& bounds, bool is_after) {
    const bool at_right_edge = is_rtl != is_after;
    return at_right_edge ? (bounds.right() - overlap / 2)
                         : (bounds.x() + overlap / 2);
  };

  int target_x = 0;
  int target_y = 0;

  if (drop_index.index < tab_strip_model()->count()) {
    tabs::TabInterface* tab =
        tab_strip_model()->GetTabAtIndex(drop_index.index);
    views::View* target_view = GetTabViewAt(drop_index.index);

    if (replace_index && target_view) {
      // When a tab is being replaced, point at the center of the tab.
      target_x = target_view->GetBoundsInScreen().CenterPoint().x();
      target_y = target_view->GetBoundsInScreen().y();
    } else if (IsDropBeforeGroupHeader(drop_index, tab)) {
      // Drop before the group header.
      views::View* header_view = GetGroupHeaderView(tab->GetGroup().value());
      views::View* anchor_view = header_view ? header_view : target_view;
      if (anchor_view) {
        gfx::Rect bounds = anchor_view->GetBoundsInScreen();
        target_x = GetAdjustedXForDrop(bounds, /*is_after=*/false);
        target_y = bounds.y();
      }
    } else if (target_view) {
      // Otherwise, point at the slot before the tab.
      gfx::Rect bounds = target_view->GetBoundsInScreen();
      target_x = GetAdjustedXForDrop(bounds, /*is_after=*/false);
      target_y = bounds.y();
    }
  } else {
    // Drop at the end of the unpinned container.
    views::View* last_view = GetTabViewAt(tab_strip_model()->count() - 1);
    if (last_view) {
      gfx::Rect bounds = last_view->GetBoundsInScreen();
      target_x = GetAdjustedXForDrop(bounds, /*is_after=*/true);
      target_y = bounds.y();
    } else if (auto* unpinned_container = GetUnpinnedTabsContainer()) {
      gfx::Rect bounds = unpinned_container->GetBoundsInScreen();
      target_x = is_rtl ? bounds.x() : bounds.right();
      target_y = bounds.y();
    }
  }

  if (target_x == 0 && target_y == 0) {
    return GetBoundsInScreen().origin();
  }

  return gfx::Point(target_x, target_y);
}

void HorizontalTabStripRegionViewNew::OnTabStripViewSet() {
  const size_t index = combo_button_ ? 1 : 0;
  ReorderChildView(tab_strip_view(), index);
}

void HorizontalTabStripRegionViewNew::UpdateButtonBorders() {
  if (!tab_strip_action_container_) {
    return;
  }
  const int extra_vertical_space =
      GetLayoutConstant(LayoutConstant::kTabStripHeight) -
      GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap) -
      TabStripControlButton::kButtonSize.height();
  const int top_inset = extra_vertical_space / 2;
  const int bottom_inset =
      extra_vertical_space - top_inset +
      GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap);

  const auto border_insets = gfx::Insets::TLBR(top_inset, 0, bottom_inset, 0);
  tab_strip_action_container_->UpdateButtonBorders(border_insets);
}

BEGIN_METADATA(HorizontalTabStripRegionViewNew)
END_METADATA

std::unique_ptr<TabStripRegionView> CreateHorizontalTabStripRegionView(
    BrowserView* browser_view) {
  if (base::FeatureList::IsEnabled(tabs::kTabStripUnification)) {
    return std::make_unique<HorizontalTabStripRegionViewNew>(browser_view);
  }
  return std::make_unique<HorizontalTabStripRegionViewOld>(browser_view);
}
