// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_button_util.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_drag_data.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/base/models/image_model.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace tab_groups {

namespace {
// The max height of the button and the max width of a button with no title.
constexpr int kButtonSize = 20;
// The corner radius for the button.
constexpr float kButtonRadius = 6.0f;
// The amount of insets above and below the text.
constexpr float kVerticalInsets = 2.0f;
// The amount of insets before and after the text.
constexpr float kHorizontalInsets = 6.0f;
// The width of the outline of the button when open in the Tab Strip.
constexpr float kBorderThickness = 1.0f;
// The size of the squircle (rounded rect) in a button with no text.
constexpr float kEmptyChipSize = 12.0f;
// The amount of padding around the squircle (rounded rect).
constexpr float kEmptyChipInsets = 4.0f;
// The radius of the squircle (rounded rect).
constexpr float kEmptyChipCornerRadius = 2.0f;
}  // namespace

SavedTabGroupButton::SavedTabGroupButton(const SavedTabGroup& group,
                                         PressedCallback callback,
                                         Browser* browser,
                                         bool animations_enabled)
    : MenuButton(std::move(callback), group.title()),
      tab_group_color_id_(group.color()),
      guid_(group.saved_guid()),
      local_group_id_(group.local_group_id()),
      tabs_(group.saved_tabs()),
      context_menu_controller_(
          this,
          base::BindRepeating(
              &SavedTabGroupUtils::CreateSavedTabGroupContextMenuModel,
              browser,
              group.saved_guid()),
          views::MenuRunner::CONTEXT_MENU | views::MenuRunner::IS_NESTED) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  GetViewAccessibility().SetName(GetAccessibleNameForButton());
  GetViewAccessibility().SetRoleDescription(l10n_util::GetStringUTF16(
      IDS_ACCNAME_SAVED_TAB_GROUP_BUTTON_ROLE_DESCRIPTION));
  SetTextProperties(group);
  SetID(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  SetProperty(views::kElementIdentifierKey, kSavedTabGroupButtonElementId);
  SetMaxSize(gfx::Size(bookmark_button_util::kMaxButtonWidth, kButtonSize));
  label()->SetTextStyle(views::style::STYLE_BODY_4_EMPHASIS);

  show_animation_ = std::make_unique<gfx::SlideAnimation>(this);
  if (!animations_enabled) {
    // For some reason during testing the events generated by animating throw
    // off the test. So, don't animate while testing.
    show_animation_->Reset(1);
  } else {
    show_animation_->Show();
  }

  ConfigureInkDropForToolbar(this);
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(0),
                                                kButtonRadius);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  set_drag_controller(this);
}

SavedTabGroupButton::~SavedTabGroupButton() = default;

void SavedTabGroupButton::UpdateButtonData(const SavedTabGroup& group) {
  SetTextProperties(group);

  tab_group_color_id_ = group.color();
  local_group_id_ = group.local_group_id();
  guid_ = group.saved_guid();
  tabs_.clear();
  tabs_ = group.saved_tabs();

  UpdateButtonLayout();
  UpdateAccessibleName();
}

std::u16string SavedTabGroupButton::GetTooltipText(const gfx::Point& p) const {
  return GetAccessibleNameForButton();
}

bool SavedTabGroupButton::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    ShowContextMenu(GetKeyboardContextMenuLocation(),
                    ui::MenuSourceType::MENU_SOURCE_KEYBOARD);
    return true;
  } else if (event.key_code() == ui::KeyboardCode::VKEY_SPACE) {
    NotifyClick(event);
    return true;
  }

  return false;
}

bool SavedTabGroupButton::IsTriggerableEvent(const ui::Event& e) {
  return e.type() == ui::EventType::kGestureTap ||
         e.type() == ui::EventType::kGestureTapDown ||
         event_utils::IsPossibleDispositionEvent(e);
}

void SavedTabGroupButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (!GetText().empty()) {
    return;
  }

  // When the title is empty, we draw a circle similar to the tab group
  // header when there is no title.
  const ui::ColorProvider* const cp = GetColorProvider();
  SkColor text_and_outline_color =
      cp->GetColor(GetSavedTabGroupOutlineColorId(tab_group_color_id_));

  // Draw squircle (rounded rect).
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(text_and_outline_color);

  canvas->DrawRoundRect(gfx::RectF(kEmptyChipInsets, kEmptyChipInsets,
                                   kEmptyChipSize, kEmptyChipSize),
                        kEmptyChipCornerRadius, flags);
}

std::u16string SavedTabGroupButton::GetAccessibleNameForButton() const {
  const std::u16string& opened_state =
      local_group_id_.has_value()
          ? l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)
          : l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_CLOSED);

  const std::u16string saved_group_acessible_name =
      GetText().empty()
          ? l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_UNNAMED_SAVED_GROUP_FORMAT, opened_state)
          : l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_NAMED_SAVED_GROUP_FORMAT, GetText(),
                opened_state);
  return saved_group_acessible_name;
}

void SavedTabGroupButton::UpdateAccessibleName() {
  GetViewAccessibility().SetName(GetAccessibleNameForButton());
}

void SavedTabGroupButton::SetText(const std::u16string& text) {
  LabelButton::SetText(text);
  UpdateAccessibleName();
}

void SavedTabGroupButton::SetTextProperties(const SavedTabGroup& group) {
  GetViewAccessibility().SetName(GetAccessibleNameForButton());
  SetTooltipText(group.title());
  SetText(group.title());
}

void SavedTabGroupButton::UpdateButtonLayout() {
  // Relies on logic in theme_helper.cc to determine dark/light palette.
  ui::ColorId background_color =
      GetTabGroupBookmarkColorId(tab_group_color_id_);

  SetEnabledTextColorIds(
      GetSavedTabGroupForegroundColorId(tab_group_color_id_));
  SetBackground(views::CreateThemedRoundedRectBackground(background_color,
                                                         kButtonRadius));

  const gfx::Insets& insets =
      gfx::Insets::VH(kVerticalInsets, kHorizontalInsets);

  // Only draw a border if the group is open in the tab strip.
  if (!local_group_id_.has_value()) {
    SetBorder(views::CreateEmptyBorder(insets));
  } else {
    std::unique_ptr<views::Border> border =
        views::CreateThemedRoundedRectBorder(
            kBorderThickness, kButtonRadius,
            GetSavedTabGroupOutlineColorId(tab_group_color_id_));
    SetBorder(views::CreatePaddedBorder(std::move(border), insets));
  }

  if (GetText().empty()) {
    // When the text is empty force the button to have square dimensions.
    SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
  } else {
    SetPreferredSize(CalculatePreferredSize({}));
  }
}

std::unique_ptr<views::LabelButtonBorder>
SavedTabGroupButton::CreateDefaultBorder() const {
  auto border = std::make_unique<views::LabelButtonBorder>();
  return border;
}

void SavedTabGroupButton::OnThemeChanged() {
  views::MenuButton::OnThemeChanged();
  UpdateButtonLayout();
}

void SavedTabGroupButton::WriteDragDataForView(View* sender,
                                               const gfx::Point& press_pt,
                                               ui::OSExchangeData* data) {
  SavedTabGroupButton* const button =
      views::AsViewClass<SavedTabGroupButton>(sender);
  CHECK(button);
  CHECK(button == this);

  // Write the image and MIME type to the OSExchangeData.
  SavedTabGroupDragData::WriteToOSExchangeData(this, press_pt,
                                               GetThemeProvider(), data);
}

int SavedTabGroupButton::GetDragOperationsForView(View* sender,
                                                  const gfx::Point& p) {
  // This may need to become more complicated
  return ui::DragDropTypes::DRAG_MOVE;
}

bool SavedTabGroupButton::CanStartDragForView(View* sender,
                                              const gfx::Point& press_pt,
                                              const gfx::Point& p) {
  // Check if we have not moved enough horizontally but we have moved downward
  // vertically - downward drag.
  gfx::Vector2d move_offset = p - press_pt;
  return View::ExceededDragThreshold(move_offset);
}

BEGIN_METADATA(SavedTabGroupButton)
END_METADATA

}  // namespace tab_groups
