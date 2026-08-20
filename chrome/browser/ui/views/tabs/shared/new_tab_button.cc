// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/new_tab_button.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/new_tab_button_menu_model.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"

namespace shared {

NewTabButton::NewTabButton(BrowserWindowInterface* browser,
                           const int button_size,
                           const int icon_size,
                           std::optional<float> corner_radius)
    : action_view_controller_(std::make_unique<views::ActionViewController>()),
      browser_(browser) {
  SetProperty(views::kElementIdentifierKey, kNewTabButtonElementId);

  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

  SetPreferredSize(gfx::Size(button_size, button_size));

  SetIconSize(icon_size);

  if (corner_radius.has_value()) {
    SetCornerRadius(corner_radius.value());
  }

  set_context_menu_controller(this);

#if BUILDFLAG(IS_LINUX)
  // On Linux, middle-clicking the New Tab Button triggers
  // paste and navigate, either to URLs or to search queries.
  SetTriggerableEventFlags(GetTriggerableEventFlags() |
                           ui::EF_MIDDLE_MOUSE_BUTTON);
#endif

  // Paint to a layer so that the ink drop is rendered correctly.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  CHECK(browser_);
  CHECK(BrowserActions::From(browser_));
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionNewTab, BrowserActions::From(browser_)->root_action_item());
  CHECK(action_item);
  action_view_controller_->CreateActionViewRelationship(
      this, action_item->GetAsWeakPtr());
}

NewTabButton::~NewTabButton() = default;

void NewTabButton::SetOnContextMenuWillShowCallback(
    base::RepeatingClosure callback) {
  on_context_menu_will_show_callback_ = std::move(callback);
}

void NewTabButton::SetOnContextMenuClosedCallback(
    base::RepeatingClosure callback) {
  on_context_menu_closed_callback_ = std::move(callback);
}

void NewTabButton::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (base::FeatureList::IsEnabled(features::kNewTabButtonContextMenu)) {
    if (on_context_menu_will_show_callback_) {
      on_context_menu_will_show_callback_.Run();
    }

    context_menu_model_ = std::make_unique<NewTabButtonMenuModel>(browser_);

    int32_t menu_runner_flags =
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;

    auto on_menu_closed = base::BindRepeating(
        &NewTabButton::OnContextMenuClosed, base::Unretained(this));

    context_menu_runner_ = std::make_unique<views::MenuRunner>(
        context_menu_model_.get(), menu_runner_flags,
        std::move(on_menu_closed));

    context_menu_runner_->RunMenuAt(
        source->GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
        views::MenuAnchorPosition::kTopLeft, source_type);
  }
}

void NewTabButton::OnContextMenuClosed() {
  if (on_context_menu_closed_callback_) {
    on_context_menu_closed_callback_.Run();
  }
}

void NewTabButton::SetMiddleClickCallbackForTesting(
    base::RepeatingClosure callback) {
  middle_click_callback_for_testing_ = std::move(callback);
}

void NewTabButton::NotifyClick(const ui::Event& event) {
#if BUILDFLAG(IS_LINUX)
  if (event.IsMouseEvent()) {
    const ui::MouseEvent& mouse = static_cast<const ui::MouseEvent&>(event);
    if (mouse.IsOnlyMiddleMouseButton()) {
      views::InkDrop::Get(this)->AnimateToState(
          views::InkDropState::ACTION_TRIGGERED,
          ui::LocatedEvent::FromIfValid(&event));
      if (middle_click_callback_for_testing_) {
        middle_click_callback_for_testing_.Run();
      } else {
        chrome::NewTabFromClipboardURL(browser_);
      }
      return;
    }
  }
#endif
  TabStripFlatEdgeButton::NotifyClick(event);
}

BEGIN_METADATA(NewTabButton)
END_METADATA

}  // namespace shared
