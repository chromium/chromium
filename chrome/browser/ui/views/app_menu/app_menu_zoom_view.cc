// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_zoom_view.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/i18n/number_formatting.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kZoomCircularButtonSize = 28;
constexpr int kZoomSeparatorPreferredLength = 24;
constexpr int kZoomButtonHorizontalInset = 9;

std::unique_ptr<views::Background> CreateZoomButtonBackground() {
  return views::CreatePillBackground(kColorAppMenuZoomButtonBackground);
}

}  // namespace

AppMenuZoomView::AppMenuZoomView(
    BrowserWindowInterface* browser_window_interface,
    views::ActionViewController* action_view_controller,
    base::flat_map<int, raw_ptr<actions::BaseAction>>& command_to_action_map,
    actions::BaseAction* zoom_row_action_item)
    : browser_window_interface_(browser_window_interface) {
  CHECK(browser_window_interface_);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  BuildZoomChildControls(zoom_row_action_item, action_view_controller,
                         command_to_action_map);

  SetBetweenChildSpacing(kZoomButtonHorizontalInset);
}

AppMenuZoomView::~AppMenuZoomView() = default;

int AppMenuZoomView::GetZoomLabelMaxWidth() const {
  const gfx::FontList& font_list = zoom_label_->font_list();
  int max_w = 0;
  content::WebContents* const selected_tab = GetActiveWebContents();
  if (selected_tab) {
    const auto* zoom_controller =
        zoom::ZoomController::FromWebContents(selected_tab);
    if (zoom_controller) {
      std::vector<double> zoom_factors =
          zoom::PageZoom::PresetZoomFactors(zoom_controller->GetZoomPercent());
      for (double zoom : zoom_factors) {
        int w = gfx::GetStringWidth(
            base::FormatPercent(static_cast<int>(std::round(zoom * 100))),
            font_list);
        max_w = std::max(w, max_w);
      }
      return max_w;
    }
  }

  // Fallback if no web_contents: check standard presets at 100% factor.
  for (double zoom : zoom::PageZoom::PresetZoomFactors(100)) {
    int w = gfx::GetStringWidth(
        base::FormatPercent(static_cast<int>(std::round(zoom * 100))),
        font_list);
    max_w = std::max(w, max_w);
  }
  return max_w;
}

void AppMenuZoomView::BuildZoomChildControls(
    actions::BaseAction* zoom_row_action_item,
    views::ActionViewController* action_view_controller,
    base::flat_map<int, raw_ptr<actions::BaseAction>>& command_to_action_map) {
  for (auto& zoom_child_holder :
       zoom_row_action_item->GetChildren().children()) {
    actions::ActionItem* zoom_child = zoom_child_holder->GetActionItem();
    actions::ActionId zoom_action_id = zoom_child->GetActionId().value();
    command_to_action_map[zoom_action_id] = zoom_child_holder.get();

    if (zoom_action_id == kActionZoomNormal) {
      zoom_label_ = AddChildView(std::make_unique<views::Label>(
          std::u16string(zoom_child->GetText())));
      zoom_label_->SetPreferredSize(gfx::Size(
          GetZoomLabelMaxWidth(), zoom_label_->GetPreferredSize().height()));

      zoom_label_subscription_ =
          zoom_child->AddActionChangedCallback(base::BindRepeating(
              [](views::Label* label, actions::ActionItem* item) {
                label->SetText(std::u16string(item->GetText()));
              },
              zoom_label_, zoom_child));
    } else {
      views::ImageButton* const zoom_child_button =
          AddChildView(CreateZoomButton(zoom_child));

      action_view_controller->CreateActionViewRelationship(
          zoom_child_button, zoom_child->GetAsWeakPtr());

      if (zoom_action_id == kActionZoomPlus) {
        auto separator = std::make_unique<views::Separator>();
        separator->SetOrientation(views::Separator::Orientation::kVertical);
        separator->SetColorId(kColorAppMenuZoomSeparator);
        separator->SetPreferredLength(kZoomSeparatorPreferredLength);
        AddChildView(std::move(separator));
      }
    }
  }

  CHECK(zoom_label_);
}

content::WebContents* AppMenuZoomView::GetActiveWebContents() const {
  tabs::TabInterface* const active_tab =
      browser_window_interface_->GetActiveTabInterface();
  return active_tab ? active_tab->GetContents() : nullptr;
}

std::unique_ptr<views::ImageButton> AppMenuZoomView::CreateZoomButton(
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

BEGIN_METADATA(AppMenuZoomView)
END_METADATA
