// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_search_bar_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/events/event_observer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

ActionAppMenuSearchBarView::ActionAppMenuSearchBarView() {
  const auto* provider = ChromeLayoutProvider::Get();
  int icon_size =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_ICON_SIZE);
  int icon_padding = 12;
  int icon_text_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);
  int left_inset = icon_padding + icon_size + icon_text_spacing;

  SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_PLACEHOLDER));
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_PLACEHOLDER));
  SetPlaceholderTextColorId(ui::kColorTextfieldForegroundPlaceholder);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(6, left_inset, 6, 12)));
  SetBackgroundColor(SK_ColorTRANSPARENT);
  SetCursorEnabled(true);

  auto search_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled()
              ? vector_icons::kSearchIcon
              : vector_icons::kSearchChromeRefreshOldIcon,
          ui::kColorIcon, icon_size));
  search_icon->SetCanProcessEventsWithinSubtree(false);
  search_icon_ = AddChildView(std::move(search_icon));

  auto* ink_drop = views::InkDrop::Get(this);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::UseInkDropForFloodFillRipple(ink_drop,
                                               /*highlight_on_hover=*/true,
                                               /*highlight_on_focus=*/true);
  ink_drop->SetBaseColor(ui::kColorSysStateHoverOnSubtle);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 8);
}

ActionAppMenuSearchBarView::~ActionAppMenuSearchBarView() = default;

void ActionAppMenuSearchBarView::AddedToWidget() {
  views::Textfield::AddedToWidget();
  SetTextfieldFocused(true);
  if (GetWidget()) {
    event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
        this, GetWidget()->GetNativeWindow(), {ui::EventType::kKeyPressed});
  }
}

void ActionAppMenuSearchBarView::RemovedFromWidget() {
  event_monitor_.reset();
  views::Textfield::RemovedFromWidget();
}

void ActionAppMenuSearchBarView::Layout(PassKey) {
  LayoutSuperclass<views::Textfield>(this);
  if (search_icon_) {
    int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_ACTION_APP_MENU_ICON_SIZE);
    int icon_x = 12;
    int icon_y = (height() - icon_size) / 2;
    search_icon_->SetBounds(icon_x, icon_y, icon_size, icon_size);
  }
}

bool ActionAppMenuSearchBarView::OnMousePressed(const ui::MouseEvent& event) {
  SetTextfieldFocused(true);
  return views::Textfield::OnMousePressed(event);
}

void ActionAppMenuSearchBarView::SetTextfieldFocused(bool focused) {
  is_active_ = focused;
  SetCursorEnabled(focused);
  if (focused) {
    RequestFocus();
  } else if (GetFocusManager()) {
    GetFocusManager()->ClearFocus();
  }
  SchedulePaint();
}

void ActionAppMenuSearchBarView::OnEvent(const ui::Event& event) {
  if (event.IsKeyEvent()) {
    ui::KeyEvent key_event = *event.AsKeyEvent();
    HandleKeyEvent(&key_event);
  }
}

void ActionAppMenuSearchBarView::HandleKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  // Only handle keys when the search bar is active/focused.
  if (!is_active_) {
    return;
  }

  const ui::KeyboardCode key_code = event->key_code();

  // Down Arrow clears focus from search bar and lets menu navigate down.
  if (key_code == ui::VKEY_DOWN) {
    SetTextfieldFocused(false);
    return;
  }

  // Leave menu action/dismissal keys for MenuController.
  if (key_code == ui::VKEY_ESCAPE || key_code == ui::VKEY_RETURN ||
      key_code == ui::VKEY_UP) {
    return;
  }

  // Typing and cursor editing (Backspace, Delete, Left/Right arrow).
  ui::TextEditCommand command = GetCommandForKeyEvent(*event);
  if (command != ui::TextEditCommand::INVALID_COMMAND) {
    ExecuteTextEditCommand(command);
  } else if (event->GetCharacter() != 0) {
    InsertChar(*event);
  }

  event->StopPropagation();
  event->SetHandled();
}

BEGIN_METADATA(ActionAppMenuSearchBarView)
END_METADATA
