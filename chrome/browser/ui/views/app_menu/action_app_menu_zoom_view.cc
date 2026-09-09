// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_zoom_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/i18n/number_formatting.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kZoomCircularButtonSize = 28;
constexpr int kZoomSeparatorPreferredLength = 24;
constexpr int kZoomButtonHorizontalInset = 9;

std::unique_ptr<views::Background> CreateZoomButtonBackground() {
  return views::CreateRoundedRectBackground(kColorAppMenuZoomButtonBackground,
                                            kZoomCircularButtonSize / 2.0f);
}

}  // namespace

ActionAppMenuZoomView::ActionAppMenuZoomView(
    BrowserWindowInterface* browser_window_interface,
    views::ActionViewController* action_view_controller,
    base::flat_map<int, raw_ptr<actions::ActionItem>>& command_to_action_map,
    actions::BaseAction* zoom_row_action_item)
    : browser_window_interface_(browser_window_interface) {
  CHECK(browser_window_interface_);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  BuildZoomChildControls(zoom_row_action_item, action_view_controller,
                         command_to_action_map);

  if (content::WebContents* const contents = GetActiveWebContents()) {
    if (auto* zoom_controller =
            zoom::ZoomController::FromWebContents(contents)) {
      zoom_observation_.Observe(zoom_controller);
    }
  }

  SetBetweenChildSpacing(kZoomButtonHorizontalInset);
  UpdateZoomControls();
  UpdateFullScreenButton();
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
    command_to_action_map[zoom_action_id] = zoom_child;

    if (zoom_action_id == kActionZoomPlus) {
      zoom_plus_button_ = zoom_child_button;
      auto separator = std::make_unique<views::Separator>();
      separator->SetOrientation(views::Separator::Orientation::kVertical);
      separator->SetColorId(kColorAppMenuZoomSeparator);
      separator->SetPreferredLength(kZoomSeparatorPreferredLength);
      AddChildView(std::move(separator));
    } else if (zoom_action_id == kActionZoomMinus) {
      zoom_minus_button_ = zoom_child_button;
      zoom_label_ = AddChildView(std::make_unique<views::Label>(
          base::FormatPercent(GetCurrentZoomPercent())));
    } else if (zoom_action_id == kActionFullscreen) {
      zoom_fullscreen_button_ = zoom_child_button;
    }
  }

  CHECK(zoom_minus_button_);
  CHECK(zoom_label_);
  CHECK(zoom_plus_button_);
  CHECK(zoom_fullscreen_button_);
}

void ActionAppMenuZoomView::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  UpdateZoomControls();
}

void ActionAppMenuZoomView::OnZoomControllerDestroyed(
    zoom::ZoomController* zoom_controller) {
  zoom_observation_.Reset();
}

void ActionAppMenuZoomView::UpdateZoomControls() {
  content::WebContents* const contents = GetActiveWebContents();
  if (!contents) {
    return;
  }
  const int zoom = GetCurrentZoomPercent();
  zoom_plus_button_->SetEnabled(zoom < contents->GetMaximumZoomPercent());
  zoom_minus_button_->SetEnabled(zoom > contents->GetMinimumZoomPercent());
  zoom_label_->SetText(base::FormatPercent(zoom));
}

void ActionAppMenuZoomView::UpdateFullScreenButton() {
  const bool is_fullscreen =
      browser_window_interface_->GetWindow() &&
      browser_window_interface_->GetWindow()->IsFullscreen();

  ExclusiveAccessManager* const exclusive_access_manager =
      ExclusiveAccessManager::From(browser_window_interface_);
  const bool can_fullscreen =
      !exclusive_access_manager || !exclusive_access_manager->context() ||
      exclusive_access_manager->context()->CanUserEnterFullscreen();

  zoom_fullscreen_button_->SetEnabled(can_fullscreen || is_fullscreen);

  const int accname_string_id =
      is_fullscreen ? IDS_ACCNAME_EXIT_FULLSCREEN
                    : (can_fullscreen ? IDS_ACCNAME_FULLSCREEN
                                      : IDS_ACCNAME_FULLSCREEN_DISABLED);
  zoom_fullscreen_button_->SetTooltipText(
      l10n_util::GetStringUTF16(accname_string_id));

  std::u16string accelerator_text;
#if !BUILDFLAG(IS_CHROMEOS)
  // ChromeOS uses a dedicated "fullscreen" media key for fullscreen mode on
  // most ChromeOS devices which cannot be specified in the standard way here,
  // so omit the accelerator to avoid providing misleading or confusing
  // information to screen reader users. See crbug.com/40708624 for context.
  ui::Accelerator fullscreen_accelerator;
  if (GetAcceleratorForCommandId(IDC_FULLSCREEN, &fullscreen_accelerator)) {
    accelerator_text = fullscreen_accelerator.GetShortcutText();
  }
#endif

  zoom_fullscreen_button_->GetViewAccessibility().SetName(
      views::MenuItemView::GetAccessibleNameForMenuItem(
          l10n_util::GetStringUTF16(accname_string_id), accelerator_text,
          /*badge_type=*/std::nullopt));
}

content::WebContents* ActionAppMenuZoomView::GetActiveWebContents() const {
  tabs::TabInterface* const active_tab =
      browser_window_interface_->GetActiveTabInterface();
  return active_tab ? active_tab->GetContents() : nullptr;
}

int ActionAppMenuZoomView::GetCurrentZoomPercent() const {
  content::WebContents* const contents = GetActiveWebContents();
  if (!contents) {
    return 100;
  }
  const auto* zoom_controller = zoom::ZoomController::FromWebContents(contents);
  return zoom_controller ? zoom_controller->GetZoomPercent() : 100;
}

std::unique_ptr<views::ImageButton> ActionAppMenuZoomView::CreateZoomButton(
    actions::ActionItem* zoom_child) {
  auto button =
      std::make_unique<views::ImageButton>(views::Button::PressedCallback());
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button->SetPreferredSize(
      gfx::Size(kZoomCircularButtonSize, kZoomCircularButtonSize));

  button->SetBackground(CreateZoomButtonBackground());
  button_subscriptions_.push_back(
      button->AddEnabledChangedCallback(base::BindRepeating(
          [](views::Button* button) {
            button->SetBackground(
                button->GetEnabled() ? CreateZoomButtonBackground() : nullptr);
          },
          button.get())));

  auto* const ink_drop = views::InkDrop::Get(button.get());
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->SetLayerRegion(views::LayerRegion::kAbove);
  ink_drop->SetBaseColor(kColorAppMenuZoomButtonHover);
  ink_drop->SetVisibleOpacity(1.0f);
  ink_drop->SetHighlightOpacity(1.0f);
  button->SetShowInkDropWhenHotTracked(true);
  views::InstallCircleHighlightPathGenerator(button.get());

  const ui::ImageModel& button_image = zoom_child->GetImage();
  // The button image can be empty during unit tests
  if (!button_image.IsEmpty() && button_image.IsVectorIcon()) {
    button->SetImageModel(views::Button::STATE_NORMAL,
                          ui::ImageModel::FromVectorIcon(
                              *button_image.GetVectorIcon().vector_icon(),
                              ui::kColorMenuItemForeground));
  }

  button->SetTooltipText(std::u16string(zoom_child->GetTooltipText()));
  button->GetViewAccessibility().SetName(
      std::u16string(zoom_child->GetTooltipText()));
  return button;
}

BEGIN_METADATA(ActionAppMenuZoomView)
END_METADATA
