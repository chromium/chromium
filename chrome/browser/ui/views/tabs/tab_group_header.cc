// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_header.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_style.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

// The amount of padding between the label and the sync icon.
constexpr int kSyncIconPaddingFromLabel = 4;

class TabGroupHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  TabGroupHighlightPathGenerator(const views::View* chip,
                                 const views::View* title,
                                 const TabGroupStyle& style)
      : chip_(chip), title_(title), style_(style) {}
  TabGroupHighlightPathGenerator(const TabGroupHighlightPathGenerator&) =
      delete;
  TabGroupHighlightPathGenerator& operator=(
      const TabGroupHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return SkPath().addRoundRect(
        gfx::RectToSkRect(chip_->bounds()),
        style_->GetHighlightPathGeneratorCornerRadius(title_),
        style_->GetHighlightPathGeneratorCornerRadius(title_));
  }

 private:
  const raw_ptr<const views::View, AcrossTasksDanglingUntriaged> chip_;
  const raw_ptr<const views::View, AcrossTasksDanglingUntriaged> title_;
  const raw_ref<const TabGroupStyle> style_;
};

}  // namespace

TabGroupHeader::TabGroupHeader(TabSlotController& tab_slot_controller,
                               const tab_groups::TabGroupId& group,
                               const TabGroupStyle& style)
    : tab_slot_controller_(tab_slot_controller),
      title_chip_(AddChildView(std::make_unique<views::View>())),
      title_(title_chip_->AddChildView(std::make_unique<views::Label>())),
      sync_icon_(
          title_chip_->AddChildView(std::make_unique<views::ImageView>())),
      saved_tab_group_service_(
          tab_slot_controller_->GetBrowser()
              ? SavedTabGroupServiceFactory::GetForProfile(
                    tab_slot_controller_->GetBrowser()->profile())
              : nullptr),
      group_style_(style),
      tab_style_(TabStyle::Get()),
      editor_bubble_tracker_(tab_slot_controller) {
  set_group(group);
  set_context_menu_controller(this);

  // Disable events processing (like tooltip handling)
  // for children of TabGroupHeader.
  title_chip_->SetCanProcessEventsWithinSubtree(false);

  title_->SetCollapseWhenHidden(true);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetElideBehavior(gfx::FADE_TAIL);

  if (features::IsChromeRefresh2023()) {
    title_->SetTextStyle(views::style::STYLE_BODY_4_EMPHASIS);
  }

  // Enable keyboard focus.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  views::FocusRing::Install(this);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<TabGroupHighlightPathGenerator>(
                title_chip_, title_, *group_style_));
  // The tab group gets painted with a solid color that may not contrast well
  // with the focus indicator, so draw an outline around the focus ring for it
  // to contrast with the solid color.
  SetProperty(views::kDrawFocusRingBackgroundOutline, true);

  SetProperty(views::kElementIdentifierKey, kTabGroupHeaderElementId);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  is_collapsed_ = tab_slot_controller_->IsGroupCollapsed(group);
}

TabGroupHeader::~TabGroupHeader() = default;

bool TabGroupHeader::OnKeyPressed(const ui::KeyEvent& event) {
  if ((event.key_code() == ui::VKEY_SPACE ||
       event.key_code() == ui::VKEY_RETURN) &&
      !editor_bubble_tracker_.is_open()) {
    tab_slot_controller_->ToggleTabGroupCollapsedState(
        group().value(), ToggleTabGroupCollapsedStateOrigin::kKeyboard);
#if BUILDFLAG(IS_WIN)
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
#else
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
#endif
    return true;
  }

  constexpr int kModifiedFlag =
#if BUILDFLAG(IS_MAC)
      ui::EF_COMMAND_DOWN;
#else
      ui::EF_CONTROL_DOWN;
#endif

  if (event.type() == ui::ET_KEY_PRESSED && (event.flags() & kModifiedFlag)) {
    if (event.key_code() == ui::VKEY_RIGHT) {
      tab_slot_controller_->ShiftGroupRight(group().value());
      return true;
    }
    if (event.key_code() == ui::VKEY_LEFT) {
      tab_slot_controller_->ShiftGroupLeft(group().value());
      return true;
    }
  }

  return false;
}

bool TabGroupHeader::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the click if the editor is already open. Do this so clicking
  // on us again doesn't re-trigger the editor.
  //
  // Though the bubble is deactivated before we receive a mouse event,
  // the actual widget destruction happens in a posted task. That task
  // gets run after we receive the mouse event. If this sounds brittle,
  // that's because it is!
  if (editor_bubble_tracker_.is_open()) {
    return false;
  }

  // Allow a right click from touch to drag, which corresponds to a long click.
  if (event.IsOnlyLeftMouseButton() ||
      (event.IsOnlyRightMouseButton() && event.flags() & ui::EF_FROM_TOUCH)) {
    tab_slot_controller_->MaybeStartDrag(
        this, event, tab_slot_controller_->GetSelectionModel());

    return true;
  }

  return false;
}

bool TabGroupHeader::OnMouseDragged(const ui::MouseEvent& event) {
  tab_slot_controller_->ContinueDrag(this, event);
  return true;
}

void TabGroupHeader::OnMouseReleased(const ui::MouseEvent& event) {
  if (!dragging()) {
    if (event.IsLeftMouseButton()) {
      tab_slot_controller_->ToggleTabGroupCollapsedState(
          group().value(), ToggleTabGroupCollapsedStateOrigin::kMouse);
    } else if (event.IsRightMouseButton() &&
               !editor_bubble_tracker_.is_open()) {
      editor_bubble_tracker_.Opened(TabGroupEditorBubbleView::Show(
          tab_slot_controller_->GetBrowser(), group().value(), this));
    }
  }

  tab_slot_controller_->EndDrag(END_DRAG_COMPLETE);
}

void TabGroupHeader::OnMouseEntered(const ui::MouseEvent& event) {
  // Hide the hover card, since there currently isn't anything to display
  // for a group.
  tab_slot_controller_->UpdateHoverCard(
      nullptr, TabSlotController::HoverCardUpdateType::kHover);
}

void TabGroupHeader::OnThemeChanged() {
  TabSlotView::OnThemeChanged();
  VisualsChanged();
}

void TabGroupHeader::OnGestureEvent(ui::GestureEvent* event) {
  tab_slot_controller_->UpdateHoverCard(
      nullptr, TabSlotController::HoverCardUpdateType::kEvent);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      tab_slot_controller_->ToggleTabGroupCollapsedState(
          group().value(), ToggleTabGroupCollapsedStateOrigin::kGesture);
      break;
    case ui::ET_GESTURE_LONG_TAP: {
      editor_bubble_tracker_.Opened(TabGroupEditorBubbleView::Show(
          tab_slot_controller_->GetBrowser(), group().value(), this));
      break;
    }
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      tab_slot_controller_->MaybeStartDrag(
          this, *event, tab_slot_controller_->GetSelectionModel());
      break;
    }
    default:
      break;
  }
  event->SetHandled();
}

void TabGroupHeader::OnFocus() {
  View::OnFocus();
  tab_slot_controller_->UpdateHoverCard(
      nullptr, TabSlotController::HoverCardUpdateType::kFocus);
}

void TabGroupHeader::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTabList;
  node_data->AddState(ax::mojom::State::kEditable);
  bool is_collapsed = tab_slot_controller_->IsGroupCollapsed(group().value());
  if (is_collapsed) {
    node_data->AddState(ax::mojom::State::kCollapsed);
    node_data->RemoveState(ax::mojom::State::kExpanded);
  } else {
    node_data->AddState(ax::mojom::State::kExpanded);
    node_data->RemoveState(ax::mojom::State::kCollapsed);
  }

  std::u16string title = tab_slot_controller_->GetGroupTitle(group().value());
  std::u16string contents =
      tab_slot_controller_->GetGroupContentString(group().value());
  std::u16string collapsed_state = std::u16string();

// Windows screen reader properly announces the state set above in |node_data|
// and will read out the state change when the header's collapsed state is
// toggled. The state is added into the title for other platforms and the title
// will be reread with the updated state when the header's collapsed state is
// toggled.
#if !BUILDFLAG(IS_WIN)
  collapsed_state =
      is_collapsed ? l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_COLLAPSED)
                   : l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_EXPANDED);
#endif
  if (title.empty()) {
    node_data->SetNameChecked(l10n_util::GetStringFUTF16(
        IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT, contents, collapsed_state));
  } else {
    node_data->SetNameChecked(
        l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_NAMED_GROUP_FORMAT, title,
                                   contents, collapsed_state));
  }
}

std::u16string TabGroupHeader::GetTooltipText(const gfx::Point& p) const {
  if (!title_->GetText().empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_GROUPS_NAMED_GROUP_TOOLTIP, title_->GetText(),
        tab_slot_controller_->GetGroupContentString(group().value()));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_GROUPS_UNNAMED_GROUP_TOOLTIP,
        tab_slot_controller_->GetGroupContentString(group().value()));
  }
}

gfx::Rect TabGroupHeader::GetAnchorBoundsInScreen() const {
  // Skip the insetting in TabSlotView::GetAnchorBoundsInScreen(). In this
  // context insetting makes the anchored bubble partially cut into the tab
  // outline.
  // TODO(crbug.com/1268481): See if the layout of TabGroupHeader can be unified
  // with tabs so that bounds do not need to be calculated differently between
  // tabs and headers. As of writing this, hover cards to not cut into the tab
  // outline but without this change TabGroupEditorBubbleView does.
  return View::GetAnchorBoundsInScreen();
}

TabSlotView::ViewType TabGroupHeader::GetTabSlotViewType() const {
  return TabSlotView::ViewType::kTabGroupHeader;
}

TabSizeInfo TabGroupHeader::GetTabSizeInfo() const {
  TabSizeInfo size_info;
  // Group headers have a fixed width based on |title_|'s width.
  const int width = GetDesiredWidth();
  size_info.pinned_tab_width = width;
  size_info.min_active_width = width;
  size_info.min_inactive_width = width;
  size_info.standard_width = width;
  return size_info;
}

void TabGroupHeader::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (editor_bubble_tracker_.is_open()) {
    return;
  }

  // When the context menu is triggered via keyboard, the keyboard event
  // propagates to the textfield inside the Editor Bubble. In those cases, we
  // want to tell the Editor Bubble to stop the event by setting
  // stop_context_menu_propagation to true.
  //
  // However, when the context menu is triggered via mouse, the same event
  // sequence doesn't happen. Stopping the context menu propagation in that case
  // would artificially hide the textfield's context menu the first time the
  // user tried to access it. So we don't want to stop the context menu
  // propagation if this call is reached via mouse.
  //
  // Notably, event behavior with a mouse is inconsistent depending on
  // OS. When not on Mac, the OnMouseReleased() event happens first and opens
  // the Editor Bubble early, preempting the Show() call below. On Mac, the
  // ShowContextMenu() event happens first and the Show() call is made here.
  //
  // So, because of the event order on non-Mac, and because there is no native
  // way to open a context menu via keyboard on Mac, we assume that we've
  // reached this function via mouse if and only if the current OS is Mac.
  // Therefore, we don't stop the menu propagation in that case.
  constexpr bool kStopContextMenuPropagation =
#if BUILDFLAG(IS_MAC)
      false;
#else
      true;
#endif

  editor_bubble_tracker_.Opened(TabGroupEditorBubbleView::Show(
      tab_slot_controller_->GetBrowser(), group().value(), this, absl::nullopt,
      nullptr, kStopContextMenuPropagation));
}

bool TabGroupHeader::DoesIntersectRect(const views::View* target,
                                       const gfx::Rect& rect) const {
  // Tab group headers are only highlighted with a tab shape while dragging, so
  // visually the header is basically a rectangle between two tab separators.
  // The distance from the endge of the view to the tab separator is half of the
  // overlap distance. We should only accept events between the separators.
  gfx::Rect contents_rect = GetLocalBounds();
  contents_rect.Inset(gfx::Insets::VH(0, tab_style_->GetTabOverlap() / 2));
  return contents_rect.Intersects(rect);
}

int TabGroupHeader::GetDesiredWidth() const {
  if (features::IsChromeRefresh2023()) {
    const int overlap_margin = group_style_->GetTabGroupViewOverlap() * 2;
    return overlap_margin + title_chip_->width();
  }

  // If the tab group is collapsed, we want the right margin of the title to
  // match the left margin. The left margin is always the group stroke inset.
  // Using these values also guarantees the chip aligns with the collapsed
  // stroke.
  if (tab_slot_controller_->IsGroupCollapsed(group().value())) {
    return title_chip_->width() + 2 * TabGroupUnderline::GetStrokeInset();
  }

  // We don't want tabs to visually overlap group headers, so we add that space
  // to the width to compensate. We don't want to actually remove the overlap
  // during layout however; that would cause an the margin to be visually uneven
  // when the header is in the first slot and thus wouldn't overlap anything to
  // the left.
  const int overlap_margin = group_style_->GetTabGroupViewOverlap() * 2;

  // The empty and non-empty chips have different sizes and corner radii, but
  // both should look nestled against the group stroke of the tab to the right.
  // This requires a +/- 2px adjustment to the width, which causes the tab to
  // the right to be positioned in the right spot. For ChromeRefresh23 the shape
  // is going to be a rounded rect for both cases and they will have the same
  // right adjust values.
  const std::u16string title =
      tab_slot_controller_->GetGroupTitle(group().value());
  const int right_adjust =
      group_style_->GetTitleAdjustmentToTabGroupHeaderDesiredWidth(title);

  return overlap_margin + title_chip_->width() + right_adjust;
}

void TabGroupHeader::VisualsChanged() {
  const tab_groups::TabGroupId tab_group_id = group().value();
  const std::u16string title =
      tab_slot_controller_->GetGroupTitle(tab_group_id);
  const tab_groups::TabGroupColorId color_id =
      tab_slot_controller_->GetGroupColorId(tab_group_id);
  const SkColor color = tab_slot_controller_->GetPaintedGroupColor(color_id);

  title_->SetText(title);

  if (ShouldShowSyncIcon()) {
    sync_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        kTabGroupsSyncIcon, color_utils::GetColorWithMaxContrast(color),
        group_style_->GetSyncIconWidth()));
  }

  sync_icon_->SetVisible(ShouldShowSyncIcon());

  if (title.empty()) {
    title_chip_->SetBoundsRect(group_style_->GetEmptyTitleChipBounds(this));
    title_chip_->SetBackground(
        group_style_->GetEmptyTitleChipBackground(color));

    if (ShouldShowSyncIcon()) {
      // The `sync_icon` should be centered in the title chip.
      gfx::Rect sync_icon_bounds = title_chip_->GetLocalBounds();
      sync_icon_bounds.ClampToCenteredSize(gfx::Size(
          group_style_->GetSyncIconWidth(), group_style_->GetSyncIconWidth()));
      sync_icon_->SetBoundsRect(sync_icon_bounds);
    } else {
      sync_icon_->SetBounds(0, 0, 0, 0);
    }
  } else {
    // If the title is set, the chip is a rounded rect that matches the active
    // tab shape, particularly the tab's corner radius.
    title_->SetEnabledColor(color_utils::GetColorWithMaxContrast(color));

    // Set the radius such that the chip nestles snugly against the tab corner
    // radius, taking into account the group underline stroke.
    const int corner_radius = group_style_->GetChipCornerRadius();

    // TODO(crbug.com/1416895): The math of the layout in this function is done
    // arithmetically and can be hard to understand. This should instead be done
    // by a layout manager.
    const int text_height = title_->GetPreferredSize().height();

    const gfx::Size sync_icon_size =
        ShouldShowSyncIcon()
            ? gfx::Size(group_style_->GetSyncIconWidth(), text_height)
            : gfx::Size();

    const int padding_between_label_sync_icon =
        ShouldShowSyncIcon() ? kSyncIconPaddingFromLabel : 0;

    // The max width of the content should be half the standard tab width (not
    // counting overlap).
    const int text_max_width =
        (tab_style_->GetStandardWidth() - tab_style_->GetTabOverlap()) / 2 -
        sync_icon_size.width() - padding_between_label_sync_icon;

    const int text_width =
        std::min(title_->GetPreferredSize().width(), text_max_width);

    // width of the content including the text label, sync icon and the padding
    // between them
    const int content_width =
        text_width + sync_icon_size.width() + padding_between_label_sync_icon;

    // horizontal and vertical insets of the title chip.
    const gfx::Insets title_chip_insets =
        group_style_->GetInsetsForHeaderChip();
    const int title_chip_vertical_inset = title_chip_insets.top();
    const int title_chip_horizontal_inset = title_chip_insets.left();

    // Width of title chip should atleast be the width of an empty title chip.
    const int title_chip_width =
        std::max(group_style_->GetEmptyTitleChipBounds(this).width(),
                 content_width + 2 * title_chip_horizontal_inset);

    // The bounds and background for the `title_chip_` is set here.
    const gfx::Point title_chip_origin =
        group_style_->GetTitleChipOffset(text_height);
    title_chip_->SetBounds(title_chip_origin.x(), title_chip_origin.y(),
                           title_chip_width,
                           text_height + 2 * title_chip_vertical_inset);
    title_chip_->SetBackground(
        views::CreateRoundedRectBackground(color, corner_radius));

    // Bounds and background of the `title_` and the `sync_icon` are set here.
    // Cannot use `title_chip_horizontal_inset` as x coordinate in the case
    // `title_chip_width` is the width of an empty title chip.
    const int start_of_sync_icon = (title_chip_->width() - content_width) / 2;
    sync_icon_->SetBounds(start_of_sync_icon, title_chip_vertical_inset,
                          sync_icon_size.width(), text_height);
    title_->SetBounds(start_of_sync_icon + sync_icon_size.width() +
                          padding_between_label_sync_icon,
                      title_chip_vertical_inset, text_width, text_height);
  }

  if (views::FocusRing::Get(this)) {
    views::FocusRing::Get(this)->Layout();
  }

  const bool collapsed_state =
      tab_slot_controller_->IsGroupCollapsed(group().value());
  if (is_collapsed_ != collapsed_state) {
    const ui::ElementIdentifier element_id =
        GetProperty(views::kElementIdentifierKey);
    if (element_id) {
      views::ElementTrackerViews::GetInstance()->NotifyViewActivated(element_id,
                                                                     this);
      is_collapsed_ = collapsed_state;
    }
  }
}

int TabGroupHeader::GetCollapsedHeaderWidth() const {
  if (features::IsChromeRefresh2023()) {
    return GetTabSizeInfo().standard_width;
  }

  const int title_adjustment =
      group_style_->GetTitleAdjustmentToTabGroupHeaderDesiredWidth(
          title_->GetText());
  const int title_chip_width = GetTabSizeInfo().standard_width -
                               2 * tab_style_->GetTabOverlap() -
                               title_adjustment;
  return title_chip_width + 2 * TabGroupUnderline::GetStrokeInset();
}

bool TabGroupHeader::ShouldShowSyncIcon() const {
  const bool is_group_saved =
      saved_tab_group_service_ && saved_tab_group_service_->model() &&
      saved_tab_group_service_->model()->Contains(group().value());
  return base::FeatureList::IsEnabled(features::kTabGroupsSave) &&
         is_group_saved;
}

void TabGroupHeader::RemoveObserverFromWidget(views::Widget* widget) {
  widget->RemoveObserver(&editor_bubble_tracker_);
}

BEGIN_METADATA(TabGroupHeader, TabSlotView)
ADD_READONLY_PROPERTY_METADATA(int, DesiredWidth)
END_METADATA

TabGroupHeader::EditorBubbleTracker::EditorBubbleTracker(
    TabSlotController& tab_slot_controller)
    : tab_slot_controller_(tab_slot_controller) {}

TabGroupHeader::EditorBubbleTracker::~EditorBubbleTracker() {
  if (is_open_) {
    widget_->RemoveObserver(this);
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    OnWidgetDestroyed(widget_);
  }
  CHECK(!IsInObserverList());
}

void TabGroupHeader::EditorBubbleTracker::Opened(views::Widget* bubble_widget) {
  DCHECK(bubble_widget);
  DCHECK(!is_open_);
  widget_ = bubble_widget;
  is_open_ = true;
  bubble_widget->AddObserver(this);
  tab_slot_controller_->NotifyTabGroupEditorBubbleOpened();
}

void TabGroupHeader::EditorBubbleTracker::OnWidgetDestroyed(
    views::Widget* widget) {
  is_open_ = false;
  tab_slot_controller_->NotifyTabGroupEditorBubbleClosed();
}
