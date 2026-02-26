// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"

#include <numeric>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_tracker.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
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
  button->SetFocusBehavior(
      VerticalTabGroupHeaderView::FocusBehavior::ACCESSIBLE_ONLY);
  button->SetVisible(false);
  views::FocusRing::Install(button);
}

void UpdateEditorButtonColors(views::LabelButton* button,
                              SkColor foreground_color) {
  views::InkDrop::Get(button)->SetBaseColor(
      color_utils::GetColorWithMaxContrast(foreground_color));
  button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kBrowserToolsChromeRefreshIcon,
                                     foreground_color, kIconSize));
  button->SetImageModel(
      views::Button::STATE_HOVERED,
      ui::ImageModel::FromVectorIcon(kBrowserToolsChromeRefreshIcon,
                                     foreground_color, kIconSize));
  button->SetImageModel(
      views::Button::STATE_PRESSED,
      ui::ImageModel::FromVectorIcon(kBrowserToolsChromeRefreshIcon,
                                     foreground_color, kIconSize));
}

class VerticalTabGroupHeaderLabel : public views::Label {
  METADATA_HEADER(VerticalTabGroupHeaderLabel, views::Label)
 public:
  VerticalTabGroupHeaderLabel() {
    SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    SetElideBehavior(gfx::FADE_TAIL);
    SetAutoColorReadabilityEnabled(false);
  }
};

BEGIN_METADATA(VerticalTabGroupHeaderLabel)
END_METADATA
}  // namespace

VerticalTabGroupHeaderView::VerticalTabGroupHeaderView(
    Delegate& delegate,
    tabs::VerticalTabStripStateController* state_controller,
    const tab_groups::TabGroupVisualData* tab_group_visual_data)
    : sync_icon_(AddChildView(std::make_unique<views::ImageView>())),
      group_header_label_(
          AddChildView(std::make_unique<VerticalTabGroupHeaderLabel>())),
      attention_indicator_(AddChildView(std::make_unique<views::ImageView>())),
      editor_bubble_button_(AddChildView(std::make_unique<views::LabelButton>(
          base::BindRepeating(&VerticalTabGroupHeaderView::ShowEditorBubble,
                              base::Unretained(this))))),
      collapse_icon_(AddChildView(std::make_unique<views::ImageView>())),
      delegate_(delegate),
      editor_bubble_tracker_(state_controller) {
  SetProperty(views::kElementIdentifierKey, kTabGroupHeaderElementId);
  SetNotifyEnterExitOnChild(true);

  ConfigureEditorBubbleButton(editor_bubble_button_);
  editor_bubble_opened_subscription_ =
      editor_bubble_tracker_.RegisterOnBubbleOpened(base::BindRepeating(
          &VerticalTabGroupHeaderView::UpdateEditorBubbleButtonVisibility,
          base::Unretained(this)));
  editor_bubble_closed_subscription_ =
      editor_bubble_tracker_.RegisterOnBubbleClosed(base::BindRepeating(
          &VerticalTabGroupHeaderView::UpdateEditorBubbleButtonVisibility,
          base::Unretained(this)));

  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  SetInteriorMargin(gfx::Insets::VH(0, kGroupHeaderHorizontalInset));
  SetDefault(views::kFlexBehaviorKey,
             views::FlexSpecification(
                 views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                 views::MaximumFlexSizeRule::kPreferred));
  // Let the header label grow to fill any extra available space but at a lower
  // order so that things like the editor bubble button will be shown over the
  // label.
  group_header_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
          views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2));
  // The collapse icon should always be seen.
  // Let the collapse icon grow to fill the remaining available space while
  // keeping the icon trailing aligned.
  collapse_icon_->SetHorizontalAlignment(
      views::ImageViewBase::Alignment::kTrailing);
  collapse_icon_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));

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

VerticalTabGroupHeaderView::~VerticalTabGroupHeaderView() = default;

bool VerticalTabGroupHeaderView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    delegate_->ToggleCollapsedState(
        ToggleTabGroupCollapsedStateOrigin::kKeyboard);
    return true;
  }
  return false;
}

bool VerticalTabGroupHeaderView::OnMousePressed(const ui::MouseEvent& event) {
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

  // Potentially start the drag for the mouse press.
  // Follow-up mouse-movement events will update the drag controller and
  // eventually kick off the drag-loop.
  delegate_->InitHeaderDrag(event);

  // Return true so that we receive subsequent MouseRelease event.
  return true;
}

bool VerticalTabGroupHeaderView::OnMouseDragged(const ui::MouseEvent& event) {
  return delegate_->ContinueHeaderDrag(event);
}

void VerticalTabGroupHeaderView::OnMouseReleased(const ui::MouseEvent& event) {
  delegate_->CancelHeaderDrag();

  bool open_editor_bubble =
      event.IsRightMouseButton() && !editor_bubble_tracker_.is_open();
  bool toggle_collapse = event.IsLeftMouseButton();
  if (open_editor_bubble) {
    ShowEditorBubble();
  } else if (toggle_collapse) {
    delegate_->ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMouse);
  }
}

void VerticalTabGroupHeaderView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureTap:
      delegate_->ToggleCollapsedState(
          ToggleTabGroupCollapsedStateOrigin::kGesture);
      break;
    case ui::EventType::kGestureLongTap:
      ShowEditorBubble();
      break;
    default:
      break;
  }
  event->SetHandled();
}

void VerticalTabGroupHeaderView::OnMouseMoved(const ui::MouseEvent& event) {
  // Linux enter/leave events are sometimes flaky, so we don't want to "miss"
  // an enter event and fail to update state.
  UpdateEditorBubbleButtonVisibility();
}

void VerticalTabGroupHeaderView::OnMouseEntered(const ui::MouseEvent& event) {
  delegate_->HideHoverCard();
  UpdateEditorBubbleButtonVisibility();
}

void VerticalTabGroupHeaderView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateEditorBubbleButtonVisibility();
}

void VerticalTabGroupHeaderView::ShowContextMenuForViewImpl(
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

  editor_bubble_tracker_.Opened(
      delegate_->ShowGroupEditorBubble(kStopContextMenuPropagation));
}

void VerticalTabGroupHeaderView::OnDataChanged(
    const tab_groups::TabGroupVisualData* tab_group_visual_data,
    bool needs_attention,
    bool is_shared) {
  group_header_label_->SetText(tab_group_visual_data->title());
  if (GetColorProvider()) {
    SkColor background_color = GetColorProvider()->GetColor(
        GetTabGroupTabStripColorId(tab_group_visual_data->color(),
                                   GetWidget()->ShouldPaintAsActive()));
    SkColor foreground_color =
        color_utils::GetColorWithMaxContrast(background_color);

    // Update label.
    group_header_label_->SetEnabledColor(foreground_color);

    // Update save tab group related items, the sync icon and attention
    // indicator.
    sync_icon_->SetVisible(is_shared);
    if (is_shared) {
      sync_icon_->SetImage(ui::ImageModel::FromVectorIcon(
          kPeopleGroupIcon, foreground_color, kIconSize));
    }
    if (tab_group_visual_data->is_collapsed() && needs_attention) {
      attention_indicator_->SetVisible(true);
      attention_indicator_->SetImage(ui::ImageModel::FromVectorIcon(
          kDefaultTouchFaviconMaskIcon, foreground_color,
          kAttentionIndicatorWidth));
    } else {
      attention_indicator_->SetVisible(false);
    }

    // Update editor bubble button.
    UpdateEditorButtonColors(editor_bubble_button_, foreground_color);

    // Update collapse icon.
    collapse_icon_->SetImage(
        ui::ImageModel::FromVectorIcon(tab_group_visual_data->is_collapsed()
                                           ? kKeyboardArrowDownChromeRefreshIcon
                                           : kKeyboardArrowUpChromeRefreshIcon,
                                       foreground_color, kIconSize));

    // Update background.
    SetBackground(views::CreateRoundedRectBackground(background_color,
                                                     kGroupHeaderCornerRadius));
  }

  UpdateIsCollapsed(tab_group_visual_data);
  UpdateAccessibleName(tab_group_visual_data, needs_attention, is_shared);
  UpdateTooltipText();
}

void VerticalTabGroupHeaderView::UpdateTooltipText() {
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

void VerticalTabGroupHeaderView::UpdateIsCollapsed(
    const tab_groups::TabGroupVisualData* tab_group_visual_data) {
  const bool is_collapsed = tab_group_visual_data->is_collapsed();
  if (is_collapsed) {
    GetViewAccessibility().SetIsCollapsed();
  } else {
    GetViewAccessibility().SetIsExpanded();
  }
}

void VerticalTabGroupHeaderView::UpdateAccessibleName(
    const tab_groups::TabGroupVisualData* tab_group_visual_data,
    bool needs_attention,
    bool is_shared) {
  const std::u16string title = tab_group_visual_data->title();

  const std::u16string contents = delegate_->GetGroupContentString();
  std::u16string group_status = std::u16string();

  // Windows screen readers reads out the collapsed state based on the
  // accessibility node data information.
#if !BUILDFLAG(IS_WIN)
  const bool is_collapsed = tab_group_visual_data->is_collapsed();
  group_status = is_collapsed
                     ? l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_COLLAPSED)
                     : l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_EXPANDED);
#endif

  std::u16string shared_state = u"";
  if (is_shared) {
    shared_state = l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_SHARED);
    if (tab_group_visual_data->is_collapsed() && needs_attention) {
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

void VerticalTabGroupHeaderView::UpdateEditorBubbleButtonVisibility() {
  editor_bubble_button_->SetVisible(editor_bubble_tracker_.is_open() ||
                                    IsMouseHovered());
}

void VerticalTabGroupHeaderView::ShowEditorBubble() {
  if (editor_bubble_tracker_.is_open()) {
    return;
  }
  editor_bubble_tracker_.Opened(delegate_->ShowGroupEditorBubble(
      /*stop_context_menu_propagation=*/false));
}

BEGIN_METADATA(VerticalTabGroupHeaderView)
END_METADATA
