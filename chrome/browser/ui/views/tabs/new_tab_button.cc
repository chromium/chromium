// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/new_tab_button.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/new_tab_button_menu_model.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"

NewTabButton::NewTabButton(PressedCallback callback,
                           const gfx::VectorIcon& icon,
                           Edge fixed_flat_edge,
                           Edge animated_flat_edge,
                           BrowserWindowInterface* browser)
    : TabStripControlButton(browser,
                            std::move(callback),
                            icon,
                            fixed_flat_edge,
                            animated_flat_edge),
      browser_(browser) {
  set_context_menu_controller(this);
  SetProperty(views::kElementIdentifierKey, kNewTabButtonElementId);
}

NewTabButton::~NewTabButton() = default;

void NewTabButton::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (base::FeatureList::IsEnabled(features::kNewTabButtonContextMenu)) {
    context_menu_model_ = std::make_unique<NewTabButtonMenuModel>(browser_);

    int32_t menu_runner_flags =
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;

    context_menu_runner_ = std::make_unique<views::MenuRunner>(
        context_menu_model_.get(), menu_runner_flags);

    context_menu_runner_->RunMenuAt(
        source->GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
        views::MenuAnchorPosition::kTopLeft, source_type);
  }
}

void NewTabButton::UpdateBackground() {
  if (features::IsGlassFrameEnabled()) {
    SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
    return;
  }
  TabStripControlButton::UpdateBackground();
}
