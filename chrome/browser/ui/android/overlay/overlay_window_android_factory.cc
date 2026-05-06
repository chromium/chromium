// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/public/browser/overlay_window.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"

std::unique_ptr<content::VideoOverlayWindow>
CreateImmersiveOverlayWindowAndroid(
    content::VideoPictureInPictureWindowController* controller);

std::unique_ptr<content::VideoOverlayWindow>
CreatePictureInPictureOverlayWindowAndroid(
    content::VideoPictureInPictureWindowController* controller);

// VideoOverlayWindow::Create is the platform-specific factory method for
// creating a video overlay window. On Android, it selects between a standard
// Picture-in-Picture window and an immersive playback window based on the
// session's immersive state.
std::unique_ptr<content::VideoOverlayWindow>
content::VideoOverlayWindow::Create(
    content::VideoPictureInPictureWindowController* controller) {
  if (controller->IsImmersive()) {
    return CreateImmersiveOverlayWindowAndroid(controller);
  }
  return CreatePictureInPictureOverlayWindowAndroid(controller);
}
