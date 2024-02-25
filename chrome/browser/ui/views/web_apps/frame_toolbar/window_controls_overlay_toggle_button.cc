// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/window_controls_overlay_toggle_button.h"

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

WindowControlsOverlayToggleButton::WindowControlsOverlayToggleButton(
    BrowserView* browser_view)
    : ToolbarButton(
          base::BindRepeating(&WindowControlsOverlayToggleButton::ButtonPressed,
                              base::Unretained(this))),
      browser_view_(browser_view) {
  UpdateState();
}

WindowControlsOverlayToggleButton::~WindowControlsOverlayToggleButton() =
    default;

void WindowControlsOverlayToggleButton::ButtonPressed(const ui::Event& event) {
  browser_view_->ToggleWindowControlsOverlayEnabled(
      base::BindOnce(&WindowControlsOverlayToggleButton::UpdateState,
                     weak_factory_.GetWeakPtr()));
}

void WindowControlsOverlayToggleButton::UpdateState() {
  // Use app_controller's IsWindowControlsOverlayEnabled rather than
  // browser_view's to avoid this returning false at startup due to the CCT
  // displaying momentarily.
  bool enabled = browser_view_->browser()
                     ->app_controller()
                     ->IsWindowControlsOverlayEnabled();

  SetVectorIcon(enabled ? kKeyboardArrowDownIcon : kKeyboardArrowUpIcon);
  SetTooltipText(l10n_util::GetStringUTF16(
      enabled ? IDS_WEB_APP_DISABLE_WINDOW_CONTROLS_OVERLAY_TOOLTIP
              : IDS_WEB_APP_ENABLE_WINDOW_CONTROLS_OVERLAY_TOOLTIP));
}

int WindowControlsOverlayToggleButton::GetIconSize() const {
  // Rather than use the default toolbar icon size, use whatever icon size is
  // embedded in the vector icon. While this matches the original implementation
  // of this class, perhaps using the default toolbar icon size would make more
  // sense.
  return 0;
}

BEGIN_METADATA(WindowControlsOverlayToggleButton)
END_METADATA
