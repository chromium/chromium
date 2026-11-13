// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/groups/ephemeral_tab_group_promotion_bubble_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/groups/color_picker_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_title_field.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// Keeps the delegate alive for as long as its Widget. The property is
// destroyed by the Widget's PropertyHandler base after ~Widget() has already
// detached the delegate (WidgetDelegate::WidgetDestroying()), so the delegate
// never outlives or dangles into the Widget.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(EphemeralTabGroupPromotionBubbleView,
                                   kEphemeralTabGroupPromotionBubbleDelegateKey)

gfx::Insets GetControlInsets() {
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  const int horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  return ui::TouchUiController::Get()->touch_ui()
             ? gfx::Insets::VH(5 * vertical_spacing / 4, horizontal_spacing)
             : gfx::Insets::VH(vertical_spacing, horizontal_spacing);
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EphemeralTabGroupPromotionBubbleView,
                                      kEphemeralTabGroupPromotionBubbleViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EphemeralTabGroupPromotionBubbleView,
                                      kCreateButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EphemeralTabGroupPromotionBubbleView,
                                      kCancelButtonId);

// static
std::unique_ptr<views::Widget> EphemeralTabGroupPromotionBubbleView::Show(
    BrowserWindowInterface* browser,
    const tab_groups::TabGroupId& group,
    views::View* anchor_view,
    std::optional<gfx::Rect> anchor_rect,
    bool stop_context_menu_propagation) {
  CHECK(anchor_view);
  CHECK(browser->GetTabStripModel()->IsEphemeralTabGroup(group));

  auto delegate = base::WrapUnique(new EphemeralTabGroupPromotionBubbleView(
      browser, group, anchor_view, anchor_rect));
  EphemeralTabGroupPromotionBubbleView* const delegate_ptr = delegate.get();
  delegate_ptr->SetContentsView(
      delegate_ptr->BuildContentsView(stop_context_menu_propagation));

  std::unique_ptr<views::Widget> widget =
      views::BubbleDialogDelegate::CreateBubble(delegate_ptr);
  delegate_ptr->GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  widget->SetProperty(kEphemeralTabGroupPromotionBubbleDelegateKey,
                      std::move(delegate));
  widget->Show();
  return widget;
}

EphemeralTabGroupPromotionBubbleView::EphemeralTabGroupPromotionBubbleView(
    BrowserWindowInterface* browser,
    const tab_groups::TabGroupId& group,
    views::View* anchor_view,
    std::optional<gfx::Rect> anchor_rect)
    : BubbleDialogDelegate(anchor_view,
                           views::BubbleBorder::Arrow::TOP_LEFT,
                           views::BubbleBorder::DIALOG_SHADOW,
                           /*autosize=*/true),
      browser_(browser),
      group_(group),
      use_set_anchor_rect_(anchor_rect.has_value()),
      colors_(ColorPickerView::GetTabGroupColors()) {
  CHECK(browser_->GetTabStripModel()->SupportsTabGroups());

  browser_close_subscription_ =
      browser_->RegisterBrowserDidClose(base::BindRepeating(
          &EphemeralTabGroupPromotionBubbleView::OnBrowserDidClose,
          base::Unretained(this)));

  if (anchor_rect) {
    SetAnchorRect(anchor_rect.value());
  }
  set_margins(gfx::Insets());
  set_adjust_if_offscreen(true);
  SetModalType(ui::mojom::ModalType::kNone);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowTitle(false);
  SetShowCloseButton(false);

  browser_->GetTabStripModel()->AddObserver(this);
}

EphemeralTabGroupPromotionBubbleView::~EphemeralTabGroupPromotionBubbleView() =
    default;

gfx::Rect EphemeralTabGroupPromotionBubbleView::GetAnchorRect() const {
  // Avoid BubbleDialogDelegate::GetAnchorRect() when an explicit anchor rect
  // was provided: the default implementation would overwrite it with the
  // anchor view's bounds, making the bubble jump when that view goes away.
  if (use_set_anchor_rect_) {
    return anchor_rect().value();
  }
  return BubbleDialogDelegate::GetAnchorRect();
}

void EphemeralTabGroupPromotionBubbleView::OnTabGroupChanged(
    const TabGroupChange& change) {
  // The ephemeral group may be removed while the bubble is open (e.g. the user
  // activates a tab outside it, exiting focus mode). The bubble has nothing to
  // act on any more, so close it.
  if (change.group == group_ && change.type == TabGroupChange::kClosed) {
    if (views::Widget* const widget = GetWidget()) {
      widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    }
  }
}

bool EphemeralTabGroupPromotionBubbleView::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  CHECK_EQ(sender, title_field());
  // Only respond to key pressed events, to be consistent with other views like
  // buttons and dialogs.
  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }
  switch (key_event.key_code()) {
    case ui::VKEY_ESCAPE:
      if (views::Widget* const widget = GetWidget()) {
        widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
      }
      return true;
    case ui::VKEY_RETURN:
      OnCreatePressed();
      return true;
    default:
      return false;
  }
}

void EphemeralTabGroupPromotionBubbleView::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (views::Widget* const widget = GetWidget()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

std::unique_ptr<views::View>
EphemeralTabGroupPromotionBubbleView::BuildContentsView(
    bool stop_context_menu_propagation) {
  const gfx::Insets control_insets = GetControlInsets();

  auto contents = std::make_unique<views::View>();
  contents->SetProperty(views::kElementIdentifierKey,
                        kEphemeralTabGroupPromotionBubbleViewId);
  contents->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(gfx::Insets::VH(control_insets.top(), 0));

  auto heading = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CONVERT_TO_GROUP),
      views::style::CONTEXT_LABEL, views::style::STYLE_HEADLINE_5);
  heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  heading->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(control_insets.top(), control_insets.left(), 0,
                        control_insets.right()));
  contents->AddChildView(std::move(heading));

  const TabGroup* const tab_group =
      browser_->GetTabStripModel()->group_model()->GetTabGroup(group_);
  auto* const added_title_field =
      contents->AddChildView(TabGroupEditorTitleField::Create(
          /*controller=*/this, tab_group->visual_data()->title(),
          stop_context_menu_propagation));
  title_field_tracker_.SetView(added_title_field);
  SetInitiallyFocusedView(added_title_field);

  // Color changes are applied only when the user presses Create, so the
  // picker does not need a selection callback.
  auto color_picker = std::make_unique<ColorPickerView>(
      colors_, tab_group->visual_data()->color(), base::DoNothing());
  color_picker->SetProperty(views::kMarginsKey,
                            gfx::Insets::VH(0, control_insets.left()));
  color_selector_tracker_.SetView(
      contents->AddChildView(std::move(color_picker)));

  contents->AddChildView(BuildButtonRow());
  return contents;
}

std::unique_ptr<views::View>
EphemeralTabGroupPromotionBubbleView::BuildButtonRow() {
  auto button_row = std::make_unique<views::View>();
  const int button_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  button_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded)
              .WithWeight(1));
  button_row->SetProperty(views::kMarginsKey, GetControlInsets());

  auto cancel_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &EphemeralTabGroupPromotionBubbleView::OnCancelPressed,
          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button->SetProperty(views::kMarginsKey,
                             gfx::Insets::TLBR(0, 0, 0, button_spacing / 2));
  cancel_button->SetProperty(views::kElementIdentifierKey, kCancelButtonId);

  auto create_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &EphemeralTabGroupPromotionBubbleView::OnCreatePressed,
          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CREATE_GROUP));
  create_button->SetStyle(ui::ButtonStyle::kProminent);
  create_button->SetProperty(views::kMarginsKey,
                             gfx::Insets::TLBR(0, button_spacing / 2, 0, 0));
  create_button->SetProperty(views::kElementIdentifierKey, kCreateButtonId);

  button_row->AddChildView(std::move(cancel_button));
  button_row->AddChildView(std::move(create_button));
  return button_row;
}

void EphemeralTabGroupPromotionBubbleView::OnCreatePressed() {
  TabStripModel* const tab_strip_model =
      browser_ ? browser_->GetTabStripModel() : nullptr;
  if (tab_strip_model && tab_strip_model->IsEphemeralTabGroup(group_)) {
    const TabGroup* const tab_group =
        tab_strip_model->group_model()->GetTabGroup(group_);
    const tab_groups::TabGroupVisualData* const current =
        tab_group->visual_data();
    ColorPickerView* const picker = color_selector();
    TabGroupEditorTitleField* const title = title_field();
    CHECK(picker);
    CHECK(title);
    const std::optional<int> selected = picker->GetSelectedElement();
    const tab_groups::TabGroupColorId color =
        selected.has_value() ? colors_[selected.value()].first
                             : current->color();

    // Apply the visuals before promoting so that observers of the promotion
    // (session, sync, tab restore) see the title and color the user chose.
    tab_strip_model->ChangeTabGroupVisuals(
        group_,
        tab_groups::TabGroupVisualData(std::u16string(title->GetText()), color,
                                       current->is_collapsed()),
        tab_group->IsCustomized());
    tab_strip_model->PromoteEphemeralTabGroup(group_);
  }

  if (views::Widget* const widget = GetWidget()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);
  }
}

void EphemeralTabGroupPromotionBubbleView::OnCancelPressed() {
  if (views::Widget* const widget = GetWidget()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kCancelButtonClicked);
  }
}

void EphemeralTabGroupPromotionBubbleView::OnBrowserDidClose(
    BrowserWindowInterface* browser) {
  browser_ = nullptr;
}

TabGroupEditorTitleField* EphemeralTabGroupPromotionBubbleView::title_field() {
  return views::AsViewClass<TabGroupEditorTitleField>(
      title_field_tracker_.view());
}

ColorPickerView* EphemeralTabGroupPromotionBubbleView::color_selector() {
  return views::AsViewClass<ColorPickerView>(color_selector_tracker_.view());
}
