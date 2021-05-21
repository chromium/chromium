// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/window_controls_overlay_toggle_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

WindowControlsOverlayToggleButton::WindowControlsOverlayToggleButton(
    BrowserView* browser_view)
    : ToolbarButton(
          base::BindRepeating(&WindowControlsOverlayToggleButton::ButtonPressed,
                              base::Unretained(this))),
      browser_view_(browser_view) {
  SetTooltipText(browser_view_->IsWindowControlsOverlayEnabled()
                     ? l10n_util::GetStringUTF16(
                           IDS_WEB_APP_ENABLE_WINDOW_CONTROLS_OVERLAY_TOOLTIP)
                     : l10n_util::GetStringUTF16(
                           IDS_WEB_APP_ENABLE_WINDOW_CONTROLS_OVERLAY_TOOLTIP));
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(kOverflowChevronIcon,
                                               gfx::kPlaceholderColor));
}

WindowControlsOverlayToggleButton::~WindowControlsOverlayToggleButton() =
    default;

void WindowControlsOverlayToggleButton::ButtonPressed(const ui::Event& event) {
  browser_view_->ToggleWindowControlsOverlayEnabled();
  SetTooltipText(browser_view_->IsWindowControlsOverlayEnabled()
                     ? l10n_util::GetStringUTF16(
                           IDS_WEB_APP_DISABLE_WINDOW_CONTROLS_OVERLAY_TOOLTIP)
                     : l10n_util::GetStringUTF16(
                           IDS_WEB_APP_ENABLE_WINDOW_CONTROLS_OVERLAY_TOOLTIP));
}
