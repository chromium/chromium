// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_accessibility.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_tracker.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_sharing/public/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kGroupHeaderCornerRadius = 8;
constexpr int kGroupHeaderHorizontalInset = 8;
constexpr int kIconSize = 16;
constexpr int kFocusRingInset = 2;
constexpr int kAttentionIndicatorWidth = 8;
// The amount of padding between the label and any sync icon.
constexpr int kSyncIconLabelPadding = 2;

views::ScrollView* GetScrollView(views::View* view) {
  views::View* ancestor = view;
  while (ancestor) {
    if (auto* scroll_view =
            views::ScrollView::GetScrollViewForContents(ancestor)) {
      return scroll_view;
    }
    ancestor = ancestor->parent();
  }
  return nullptr;
}

void ConfigureEditorBubbleButton(views::LabelButton* button) {
  button->SetHasInkDropActionOnClick(true);

  // Inkdrop configuration
  auto highlight_path =
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets());

  highlight_path->set_use_contents_bounds(true);
  views::HighlightPathGenerator::Install(button, std::move(highlight_path));

  views::InkDrop::Get(button)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(button)->SetHighlightOpacity(0.2f);
  views::InkDrop::Get(button)->SetVisibleOpacity(0.08f);

  button->button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_VERTICAL_TAB_GROUP_MORE_OPTIONS));
  button->SetFocusBehavior(TabGroupHeaderView::FocusBehavior::ACCESSIBLE_ONLY);
  button->SetVisible(false);
  views::FocusRing::Install(button);
  button->SetProperty(views::kElementIdentifierKey,
                      kTabGroupEditorBubbleButtonElementId);
}

void UpdateEditorButtonColors(views::LabelButton* button,
                              SkColor foreground_color) {
  views::InkDrop::Get(button)->SetBaseColor(
      color_utils::GetColorWithMaxContrast(foreground_color));
  button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? kMoreVertIcon
                                         : kBrowserToolsChromeRefreshOldIcon,
                                     foreground_color, kIconSize));
  button->SetImageModel(
      views::Button::STATE_HOVERED,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? kMoreVertIcon
                                         : kBrowserToolsChromeRefreshOldIcon,
                                     foreground_color, kIconSize));
  button->SetImageModel(
      views::Button::STATE_PRESSED,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? kMoreVertIcon
                                         : kBrowserToolsChromeRefreshOldIcon,
                                     foreground_color, kIconSize));
}

class TabGroupHeaderLabel : public views::Label {
  METADATA_HEADER(TabGroupHeaderLabel, views::Label)
 public:
  TabGroupHeaderLabel() {
    SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    SetElideBehavior(gfx::FADE_TAIL);
    SetAutoColorReadabilityEnabled(false);
  }
};

BEGIN_METADATA(TabGroupHeaderLabel)
END_METADATA
}  // namespace

TabGroupHeaderView::TabGroupHeaderView(
    Delegate& delegate,
    TabStripOrientation orientation,
    tabs::VerticalTabStripStateController* state_controller,
    const tab_groups::TabGroupVisualData* tab_group_visual_data)
    : HoverCardAnchorTarget(this),
      tab_group_visual_data_(*tab_group_visual_data),
      sync_icon_(AddChildView(std::make_unique<views::ImageView>())),
      group_header_label_(
          AddChildView(std::make_unique<TabGroupHeaderLabel>())),
      attention_indicator_(AddChildView(std::make_unique<views::ImageView>())),
      editor_bubble_button_(
          orientation == TabStripOrientation::kVertical
              ? AddChildView(std::make_unique<views::LabelButton>(
                    base::BindRepeating(&TabGroupHeaderView::ShowEditorBubble,
                                        base::Unretained(this))))
              : nullptr),
      collapse_icon_(orientation == TabStripOrientation::kVertical
                         ? AddChildView(std::make_unique<views::ImageView>())
                         : nullptr),
      delegate_(delegate),
      orientation_(orientation),
      editor_bubble_tracker_(state_controller) {
  SetProperty(views::kElementIdentifierKey, kTabGroupHeaderElementId);
  attention_indicator_->SetProperty(views::kElementIdentifierKey,
                                    kTabGroupHeaderAttentionIndicatorElementId);
  SetNotifyEnterExitOnChild(true);

  if (editor_bubble_button_) {
    ConfigureEditorBubbleButton(editor_bubble_button_);
    editor_bubble_button_->SetVisible(delegate_->IsGroupFocused());
  }
  editor_bubble_opened_subscription_ =
      editor_bubble_tracker_.RegisterOnBubbleOpened(base::BindRepeating(
          &TabGroupHeaderView::OnBubbleOpened, base::Unretained(this)));
  editor_bubble_closed_subscription_ =
      editor_bubble_tracker_.RegisterOnBubbleClosed(base::BindRepeating(
          &TabGroupHeaderView::OnBubbleClosed, base::Unretained(this)));

  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  SetInteriorMargin(gfx::Insets::VH(0, kGroupHeaderHorizontalInset));
  SetDefault(views::kFlexBehaviorKey,
             views::FlexSpecification(
                 views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                 views::MaximumFlexSizeRule::kPreferred));
  // Let the header label grow to fill any extra available space but at a lower
  // order so that things like the editor bubble button will be shown over the
  // label. Set the cross axis minimum flex size rule to kScaleToZero so that
  // as much of the label shows as possible if the height is taller than the max
  // height.
  group_header_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::LayoutOrientation::kHorizontal,
          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
          views::MaximumFlexSizeRule::kUnbounded,
          /*adjust_height_for_width=*/false,
          views::MinimumFlexSizeRule::kScaleToZero)
          .WithOrder(2));
  // The collapse icon should be seen when not in focus mode.
  // Let the collapse icon grow to fill the remaining available space while
  // keeping the icon trailing aligned.
  if (collapse_icon_) {
    collapse_icon_->SetHorizontalAlignment(
        views::ImageViewBase::Alignment::kTrailing);
    collapse_icon_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kPreferred));
    collapse_icon_->SetVisible(!delegate_->IsGroupFocused());
  }

  sync_icon_->SetProperty(views::kMarginsKey,
                          gfx::Insets::TLBR(0, 0, 0, kSyncIconLabelPadding));
  attention_indicator_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, kSyncIconLabelPadding, 0, 0));

  // Add accessibility and focus ring for the header.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  GetViewAccessibility().SetIsEditable(true);
  views::FocusRing::Install(this);
  GetViewAccessibility().SetName(
      std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  // Rounds the corners of the focus ring to match the header's shape
  views::HighlightPathGenerator::Install(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(kFocusRingInset), kGroupHeaderCornerRadius));
}

TabGroupHeaderView::~TabGroupHeaderView() = default;

gfx::Size TabGroupHeaderView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size =
      views::FlexLayoutView::CalculatePreferredSize(available_size);
  if (orientation_ == TabStripOrientation::kVertical) {
    return size;
  }

  // The horizontal layout's preferred size is dynamic based on the label and if
  // the sync icons are showing.
  int sync_icon_width_with_margin = 0;
  if (sync_icon_ && sync_icon_->GetVisible()) {
    const auto* margins = sync_icon_->GetProperty(views::kMarginsKey);
    sync_icon_width_with_margin = sync_icon_->GetPreferredSize().width() +
                                  (margins ? margins->width() : 0);
  }

  int non_label_width = 0;
  if (sync_icon_ && sync_icon_->GetVisible()) {
    non_label_width += sync_icon_width_with_margin;
  }
  if (attention_indicator_ && attention_indicator_->GetVisible()) {
    const auto* margins = attention_indicator_->GetProperty(views::kMarginsKey);
    non_label_width += attention_indicator_->GetPreferredSize().width() +
                       (margins ? margins->width() : 0);
  }
  non_label_width += 2 * kGroupHeaderHorizontalInset;

  int label_width =
      group_header_label_ ? group_header_label_->GetPreferredSize().width() : 0;
  const TabStyle* tab_style = TabStyle::Get();
  const int text_max_width = (tab_style->GetStandardWidth(/*is_split*/ false) -
                              tab_style->GetTabOverlap()) /
                                 2 -
                             sync_icon_width_with_margin;
  label_width = std::min(label_width, text_max_width);

  size.set_width(non_label_width + label_width);
  return size;
}

bool TabGroupHeaderView::OnKeyPressed(const ui::KeyEvent& event) {
  if ((event.key_code() == ui::VKEY_SPACE ||
       event.key_code() == ui::VKEY_RETURN) &&
      !delegate_->IsGroupFocused()) {
    delegate_->ToggleCollapsedState(
        ToggleTabGroupCollapsedStateOrigin::kKeyboard);
    views::ElementTrackerViews::GetInstance()->NotifyViewActivated(
        kTabGroupHeaderElementId, this);
    return true;
  }

  std::optional<event_utils::ReorderDirection> reorder_direction =
      event_utils::GetReorderCommandForKeyboardEvent(
          event, views::LayoutOrientation::kVertical);
  if (!reorder_direction) {
    return false;
  }

  switch (*reorder_direction) {
    case event_utils::ReorderDirection::kPrevious: {
      delegate_->ShiftGroupUp();
      return true;
    }
    case event_utils::ReorderDirection::kNext: {
      delegate_->ShiftGroupDown();
      return true;
    }
  }

  return false;
}

bool TabGroupHeaderView::OnMousePressed(const ui::MouseEvent& event) {
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

  // Hide the group hovercard if it is currently showing.
  delegate_->HideHoverCard(TabSlotController::HoverCardUpdateType::kEvent);

  // Potentially start the drag for the mouse press.
  // Follow-up mouse-movement events will update the drag controller and
  // eventually kick off the drag-loop.
  delegate_->InitHeaderDrag(event);

  // Return true so that we receive subsequent MouseRelease event.
  return true;
}

bool TabGroupHeaderView::OnMouseDragged(const ui::MouseEvent& event) {
  return delegate_->ContinueHeaderDrag(event);
}

void TabGroupHeaderView::OnMouseReleased(const ui::MouseEvent& event) {
  delegate_->CancelHeaderDrag();

  bool open_editor_bubble =
      event.IsRightMouseButton() && !editor_bubble_tracker_.is_open();
  bool toggle_collapse =
      event.IsLeftMouseButton() && !delegate_->IsGroupFocused();
  if (open_editor_bubble) {
    ShowEditorBubble();
  } else if (toggle_collapse) {
    delegate_->ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMouse);
    views::ElementTrackerViews::GetInstance()->NotifyViewActivated(
        kTabGroupHeaderElementId, this);
  }
}

void TabGroupHeaderView::OnGestureEvent(ui::GestureEvent* event) {
  delegate_->HideHoverCard(TabSlotController::HoverCardUpdateType::kEvent);

  switch (event->type()) {
    case ui::EventType::kGestureTapDown:
      // Required to allow the touch system to know this is a gesture target
      // for subsequent events like LongPress and Scroll.
      event->SetHandled();
      break;

    case ui::EventType::kGestureTap:
      if (!delegate_->IsGroupFocused()) {
        delegate_->ToggleCollapsedState(
            ToggleTabGroupCollapsedStateOrigin::kGesture);
        views::ElementTrackerViews::GetInstance()->NotifyViewActivated(
            kTabGroupHeaderElementId, this);
      }
      event->SetHandled();
      break;

    case ui::EventType::kGestureLongPress:
      delegate_->InitHeaderDrag(*event);
      event->SetHandled();
      break;

    case ui::EventType::kGestureLongTap:
      ShowEditorBubble();
      event->SetHandled();
      break;

    default:
      break;
  }
}

void TabGroupHeaderView::OnMouseMoved(const ui::MouseEvent& event) {
  // Linux enter/leave events are sometimes flaky, so we don't want to "miss"
  // an enter event and fail to update state.
  UpdateEditorBubbleButtonVisibility();
}

void TabGroupHeaderView::OnMouseEntered(const ui::MouseEvent& event) {
  if (features::IsTabGroupHoverCardsEnabled()) {
    delegate_->UpdateHoverCard(TabSlotController::HoverCardUpdateType::kHover);
  } else {
    delegate_->HideHoverCard(TabSlotController::HoverCardUpdateType::kHover);
  }
  UpdateEditorBubbleButtonVisibility();
}

void TabGroupHeaderView::OnMouseExited(const ui::MouseEvent& event) {
#if BUILDFLAG(IS_LINUX)
  // Bypasses the synchronous IsMouseHovered() check which can be stale on Linux
  // Wayland/X11 due to asynchronous cursor updates during mouse exit events.
  SetEditorBubbleButtonVisibilityOnHover(/*is_hovered=*/false);
#else
  UpdateEditorBubbleButtonVisibility();
#endif
}

void TabGroupHeaderView::OnFocus() {
  UpdateEditorBubbleButtonVisibility();
  if (features::IsTabGroupHoverCardsEnabled()) {
    delegate_->UpdateHoverCard(TabSlotController::HoverCardUpdateType::kFocus);
  }
}

void TabGroupHeaderView::OnBlur() {
  UpdateEditorBubbleButtonVisibility();
  if (features::IsTabGroupHoverCardsEnabled() &&
      !delegate_->IsFocusInTabStrip()) {
    delegate_->HideHoverCard(TabSlotController::HoverCardUpdateType::kFocus);
  }
}

void TabGroupHeaderView::AddedToWidget() {
  views::FlexLayoutView::AddedToWidget();
  GetFocusManager()->AddFocusChangeListener(this);
  editor_bubble_tracker_.SetScrollView(GetScrollView(this));
}

void TabGroupHeaderView::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
  views::FlexLayoutView::RemovedFromWidget();
}

void TabGroupHeaderView::OnWillChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {
  if (Contains(focused_now)) {
    UpdateEditorBubbleButtonVisibility();
    // If navigating upward from below, the button is initially hidden and gets
    // skipped. We detect reverse focus traversal (from a view physically below
    // this one) and manually forward the focus to the button.
    if (focused_now == this && focused_before &&
        focused_before->GetBoundsInScreen().y() > GetBoundsInScreen().y()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<TabGroupHeaderView> view) {
                if (view && view->editor_bubble_button_) {
                  view->GetFocusManager()->SetFocusedViewWithReason(
                      view->editor_bubble_button_,
                      views::FocusManager::FocusChangeReason::kFocusTraversal);
                }
              },
              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void TabGroupHeaderView::OnDidChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {
  if (Contains(focused_before) && !Contains(focused_now)) {
    UpdateEditorBubbleButtonVisibility();
  }
}

void TabGroupHeaderView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
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

  if (!expand_on_hover_lock_) {
    expand_on_hover_lock_ = delegate_->AcquireExpandOnHoverLock();
  }

  editor_bubble_tracker_.Opened(
      delegate_->ShowGroupEditorBubble(kStopContextMenuPropagation));
}

bool TabGroupHeaderView::NeedsToShowThumbnail() const {
  return false;
}

bool TabGroupHeaderView::IsValidHoverCardTarget() const {
  return delegate_->IsValid();
}

views::BubbleAnchor TabGroupHeaderView::GetAnchor() {
  return views::BubbleAnchor(this);
}

views::BubbleBorder::Arrow TabGroupHeaderView::GetAnchorPosition() const {
  return views::BubbleBorder::LEFT_TOP;
}

tab_groups::TabGroupId TabGroupHeaderView::group() const {
  return delegate_->GetTabGroup().id();
}

void TabGroupHeaderView::OnDataChanged(
    const tabs::TabGroupData& tab_group_data) {
  tab_group_visual_data_ = tab_group_data.visual_data;
  is_shared_ = tab_group_data.is_sharing_group;

  group_header_label_->SetText(tab_group_visual_data_.title());
  if (GetColorProvider()) {
    SkColor background_color = GetColorProvider()->GetColor(
        GetTabGroupTabStripColorId(tab_group_visual_data_.color(),
                                   GetWidget()->ShouldPaintAsActive()));
    SkColor foreground_color = GetForegroundColor();

    // Update label.
    group_header_label_->SetEnabledColor(foreground_color);

    // Update save tab group related items, the sync icon and attention
    // indicator.
    sync_icon_->SetVisible(is_shared_);
    if (is_shared_) {
      sync_icon_->SetImage(ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kGroupCustomIcon
                                            : kPeopleGroupOldIcon,
          foreground_color, kIconSize));
    }
    if (tab_group_visual_data_.is_collapsed() && needs_attention_) {
      attention_indicator_->SetVisible(true);
      attention_indicator_->SetImage(ui::ImageModel::FromVectorIcon(
          kDefaultTouchFaviconMaskCustomIcon, foreground_color,
          kAttentionIndicatorWidth));
    } else {
      attention_indicator_->SetVisible(false);
    }

    // Update editor bubble button.
    if (editor_bubble_button_) {
      UpdateEditorButtonColors(editor_bubble_button_, foreground_color);
      UpdateEditorBubbleButtonVisibility();
    }

    // Update collapse icon.
    if (collapse_icon_) {
      collapse_icon_->SetVisible(!delegate_->IsGroupFocused());
      collapse_icon_->SetImage(ui::ImageModel::FromVectorIcon(
          tab_group_visual_data_.is_collapsed()
              ? features::IsRoundedIconsEnabled()
                    ? kKeyboardArrowDownIcon
                    : kKeyboardArrowDownChromeRefreshOldIcon
          : features::IsRoundedIconsEnabled()
              ? vector_icons::kKeyboardArrowUpIcon
              : kKeyboardArrowUpChromeRefreshOldIcon,
          foreground_color, kIconSize));
    }

    // Update background.
    SetBackground(views::CreateRoundedRectBackground(background_color,
                                                     kGroupHeaderCornerRadius));
    UpdateAttentionState(
        data_sharing::features::IsDataSharingFunctionalityEnabled() &&
        tab_group_data.needs_attention);
  }

  UpdateIsCollapsed();
  UpdateAccessibleName();

  if (features::IsTabGroupHoverCardsEnabled()) {
    SetHoverCardDataFrom(tab_group_data);
  } else {
    UpdateTooltipText();
  }
}

void TabGroupHeaderView::UpdateTooltipText() {
  if (group_header_label_->GetText().empty()) {
    SetTooltipText(
        l10n_util::GetStringFUTF16(IDS_TAB_GROUPS_UNNAMED_GROUP_TOOLTIP,
                                   delegate_->GetGroupContentString()));
  } else {
    SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_TAB_GROUPS_NAMED_GROUP_TOOLTIP,
        std::u16string(group_header_label_->GetText()),
        delegate_->GetGroupContentString()));
  }
}

void TabGroupHeaderView::UpdateIsCollapsed() {
  const bool is_collapsed = tab_group_visual_data_.is_collapsed();
  if (is_collapsed) {
    GetViewAccessibility().SetIsCollapsed();
  } else {
    GetViewAccessibility().SetIsExpanded();
  }
}

void TabGroupHeaderView::UpdateAttentionState(bool needs_attention) {
  if (needs_attention_ == needs_attention) {
    return;
  }
  needs_attention_ = needs_attention;

  if (tab_group_visual_data_.is_collapsed() && needs_attention_) {
    attention_indicator_->SetVisible(true);
    attention_indicator_->SetImage(ui::ImageModel::FromVectorIcon(
        kDefaultTouchFaviconMaskCustomIcon, GetForegroundColor(),
        kAttentionIndicatorWidth));
  } else {
    attention_indicator_->SetVisible(false);
  }

  UpdateAccessibleName();
}

void TabGroupHeaderView::UpdateAccessibleName() {
  if (features::IsTabGroupHoverCardsEnabled()) {
    GetViewAccessibility().SetName(tab_groups::GetHoverCardAccessibilityText(
        delegate_->GetTabGroupData()));
    return;
  }

  const std::u16string title = tab_group_visual_data_.title();

  const std::u16string contents = delegate_->GetGroupContentString();
  std::u16string group_status = std::u16string();

  // Windows screen readers reads out the collapsed state based on the
  // accessibility node data information.
#if !BUILDFLAG(IS_WIN)
  const bool is_collapsed = tab_group_visual_data_.is_collapsed();
  group_status = is_collapsed
                     ? l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_COLLAPSED)
                     : l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_EXPANDED);
#endif

  std::u16string shared_state = u"";
  if (is_shared_) {
    shared_state = l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_SHARED);
    if (tab_group_visual_data_.is_collapsed() && needs_attention_) {
      shared_state += u", " + l10n_util::GetStringUTF16(
                                  DATA_SHARING_GROUP_LABEL_NEW_ACTIVITY);
    }
  }

  std::u16string final_name;
  if (title.empty()) {
    final_name =
        l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                   shared_state, contents, group_status);
  } else {
    final_name =
        l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_NAMED_GROUP_FORMAT,
                                   shared_state, title, contents, group_status);
  }

  GetViewAccessibility().SetName(final_name);
}

SkColor TabGroupHeaderView::GetForegroundColor() const {
  if (GetColorProvider()) {
    SkColor background_color = GetColorProvider()->GetColor(
        GetTabGroupTabStripColorId(tab_group_visual_data_.color(),
                                   GetWidget()->ShouldPaintAsActive()));
    return color_utils::GetColorWithMaxContrast(background_color);
  }
  return SK_ColorBLACK;
}

void TabGroupHeaderView::UpdateEditorBubbleButtonVisibility() {
  // The editor bubble button only exists in the vertical orientation.
  if (!editor_bubble_button_) {
    return;
  }
  views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    return;
  }

  SetEditorBubbleButtonVisibilityOnHover(
      IsMouseHovered() || Contains(focus_manager->GetFocusedView()));
}

void TabGroupHeaderView::OnBubbleOpened() {
  if (!expand_on_hover_lock_) {
    expand_on_hover_lock_ = delegate_->AcquireExpandOnHoverLock();
  }

  UpdateEditorBubbleButtonVisibility();
}

void TabGroupHeaderView::OnBubbleClosed() {
  expand_on_hover_lock_.reset();
  UpdateEditorBubbleButtonVisibility();
}

void TabGroupHeaderView::SetEditorBubbleButtonVisibilityOnHover(
    bool is_hovered) {
  if (!editor_bubble_button_) {
    return;
  }
  editor_bubble_button_->SetVisible(delegate_->IsGroupFocused() ||
                                    editor_bubble_tracker_.is_open() ||
                                    is_hovered);
}

void TabGroupHeaderView::ShowEditorBubble() {
  if (editor_bubble_tracker_.is_open()) {
    return;
  }
  if (!expand_on_hover_lock_) {
    expand_on_hover_lock_ = delegate_->AcquireExpandOnHoverLock();
  }

  editor_bubble_tracker_.Opened(delegate_->ShowGroupEditorBubble(
      /*stop_context_menu_propagation=*/false));
}

BEGIN_METADATA(TabGroupHeaderView)
END_METADATA
