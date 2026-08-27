// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_zoom_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/i18n/number_formatting.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"

namespace {

constexpr int kSeparatorPreferredLength = 16;

}  // namespace

ActionAppMenuZoomView::ActionAppMenuZoomView(
    BrowserWindowInterface* browser_window_interface,
    views::ActionViewController* action_view_controller,
    base::flat_map<int, raw_ptr<actions::ActionItem>>& command_to_action_map,
    actions::BaseAction* zoom_row_action_item)
    : browser_window_interface_(browser_window_interface) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  BuildZoomChildControls(zoom_row_action_item, action_view_controller,
                         command_to_action_map);

  if (browser_window_interface_) {
    tabs::TabInterface* active_tab =
        browser_window_interface_->GetActiveTabInterface();
    content::WebContents* contents =
        active_tab ? active_tab->GetContents() : nullptr;
    if (contents) {
      if (auto* zoom_controller =
              zoom::ZoomController::FromWebContents(contents)) {
        zoom_observation_.Observe(zoom_controller);
      }
    }
  }
}

ActionAppMenuZoomView::~ActionAppMenuZoomView() = default;

void ActionAppMenuZoomView::BuildZoomChildControls(
    actions::BaseAction* zoom_row_action_item,
    views::ActionViewController* action_view_controller,
    base::flat_map<int, raw_ptr<actions::ActionItem>>& command_to_action_map) {
  for (auto& zoom_child_holder :
       zoom_row_action_item->GetChildren().children()) {
    actions::ActionItem* zoom_child = zoom_child_holder->GetActionItem();
    actions::ActionId zoom_action_id = zoom_child->GetActionId().value();

    views::ImageButton* const zoom_child_button =
        AddChildView(CreateZoomButton(zoom_child));

    action_view_controller->CreateActionViewRelationship(
        zoom_child_button, zoom_child->GetAsWeakPtr());
    command_to_action_map[zoom_child->GetActionId().value()] = zoom_child;

    if (zoom_action_id == kActionZoomPlus) {
      auto separator = std::make_unique<views::Separator>();
      separator->SetOrientation(views::Separator::Orientation::kVertical);
      separator->SetColorId(ui::kColorMenuSeparator);
      separator->SetPreferredLength(kSeparatorPreferredLength);
      AddChildView(std::move(separator));
    } else if (zoom_action_id == kActionZoomMinus) {
      zoom_label_ = AddChildView(std::make_unique<views::Label>(
          base::FormatPercent(GetCurrentZoomPercent())));
    }
  }
}

void ActionAppMenuZoomView::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  if (zoom_label_) {
    zoom_label_->SetText(base::FormatPercent(GetCurrentZoomPercent()));
  }
}

void ActionAppMenuZoomView::OnZoomControllerDestroyed(
    zoom::ZoomController* zoom_controller) {
  zoom_observation_.Reset();
}

int ActionAppMenuZoomView::GetCurrentZoomPercent() const {
  if (!browser_window_interface_) {
    return 100;
  }
  tabs::TabInterface* const active_tab =
      browser_window_interface_->GetActiveTabInterface();
  content::WebContents* const contents =
      active_tab ? active_tab->GetContents() : nullptr;
  if (!contents) {
    return 100;
  }
  const auto* zoom_controller = zoom::ZoomController::FromWebContents(contents);
  return zoom_controller->GetZoomPercent();
}

std::unique_ptr<views::ImageButton> ActionAppMenuZoomView::CreateZoomButton(
    actions::ActionItem* zoom_child) {
  auto button =
      std::make_unique<views::ImageButton>(views::Button::PressedCallback());

  if (!zoom_child->GetImage().IsEmpty() &&
      zoom_child->GetImage().IsVectorIcon()) {
    const gfx::VectorIcon* icon =
        zoom_child->GetImage().GetVectorIcon().vector_icon();
    button->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(*icon, ui::kColorMenuItemForeground));
    button->SetImageModel(views::Button::STATE_HOVERED,
                          ui::ImageModel::FromVectorIcon(
                              *icon, ui::kColorMenuItemForegroundSelected));
    button->SetImageModel(views::Button::STATE_PRESSED,
                          ui::ImageModel::FromVectorIcon(
                              *icon, ui::kColorMenuItemForegroundSelected));
  }

  button->SetTooltipText(std::u16string(zoom_child->GetTooltipText()));
  button->GetViewAccessibility().SetName(
      std::u16string(zoom_child->GetTooltipText()));
  return button;
}

BEGIN_METADATA(ActionAppMenuZoomView)
END_METADATA
