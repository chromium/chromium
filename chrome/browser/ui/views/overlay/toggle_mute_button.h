// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_TOGGLE_MUTE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_TOGGLE_MUTE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

// A toggle button to mute/unmute media audio in the Picture-in-Picture window.
class ToggleMuteButton : public OverlayWindowImageButton {
  METADATA_HEADER(ToggleMuteButton, OverlayWindowImageButton)

 public:
  explicit ToggleMuteButton(PressedCallback callback);
  ToggleMuteButton(const ToggleMuteButton&) = delete;
  ToggleMuteButton& operator=(const ToggleMuteButton&) = delete;
  ~ToggleMuteButton() override = default;

  void SetMutedState(bool is_muted);

  bool is_muted_for_testing() const { return is_muted_; }

 protected:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdateImageAndTooltipText();

  bool is_muted_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_TOGGLE_MUTE_BUTTON_H_
