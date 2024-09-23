// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_VIEW_H_
#define COMPONENTS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

// FullscreenControlView shows a FAB (floating action button from the material
// design spec) with close icon (i.e. a partially-transparent black circular
// button with a "X" icon in the middle).
// |callback| will be called when the user taps the button.
class FullscreenControlView : public views::View {
  METADATA_HEADER(FullscreenControlView, views::View)

 public:
  explicit FullscreenControlView(views::Button::PressedCallback callback);
  FullscreenControlView(const FullscreenControlView&) = delete;
  FullscreenControlView& operator=(const FullscreenControlView&) = delete;
  ~FullscreenControlView() override;

  static constexpr int kCircleButtonDiameter = 48;

  views::Button* exit_fullscreen_button_for_testing() {
    return exit_fullscreen_button_;
  }

 private:
  raw_ptr<views::Button> exit_fullscreen_button_;
};

#endif  // COMPONENTS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_VIEW_H_
