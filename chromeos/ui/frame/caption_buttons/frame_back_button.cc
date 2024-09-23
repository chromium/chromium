// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/caption_buttons/frame_back_button.h"

#include <tuple>

#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"

namespace chromeos {

FrameBackButton::FrameBackButton()
    : FrameCaptionButton(base::BindRepeating(&FrameBackButton::ButtonPressed,
                                             base::Unretained(this)),
                         views::CAPTION_BUTTON_ICON_BACK,
                         HTMENU) {
  SetPreferredSize(views::GetCaptionButtonLayoutSize(
      views::CaptionButtonLayoutSize::kNonBrowserCaption));
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_BACK));
}

FrameBackButton::~FrameBackButton() = default;

void FrameBackButton::ButtonPressed() {
  // Send up event as well as down event as ARC++ clients expect this sequence.
  // TODO: Investigate if we should be using the current modifiers.
  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::KeyEvent press_key_event(ui::EventType::kKeyPressed,
                               ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  std::ignore = root_window->GetHost()->SendEventToSink(&press_key_event);
  ui::KeyEvent release_key_event(ui::EventType::kKeyReleased,
                                 ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  std::ignore = root_window->GetHost()->SendEventToSink(&release_key_event);
}

}  // namespace chromeos
