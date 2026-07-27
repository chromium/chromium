// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_H_

#include "ui/views/bubble/bubble_anchor.h"

class MediaToolbarButtonController;

// An abstract interface for the Global Media Controls toolbar button.
// This interface allows callers to interact with the media icon without needing
// to depend on the concrete Views or WebUI implementations.
class MediaToolbarButton {
 public:
  virtual ~MediaToolbarButton() = default;

  // Returns the bubble anchor to be used when positioning Global Media Control
  // dialogs or prompts relative to the media button.
  virtual views::BubbleAnchor GetBubbleAnchor() = 0;

  // Returns the controller responsible for managing the visibility and state
  // of the media button in response to media notifications and sessions.
  virtual MediaToolbarButtonController* GetController() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_H_
