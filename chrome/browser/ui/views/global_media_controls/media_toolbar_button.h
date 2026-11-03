// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_H_

#include "base/observer_list.h"
#include "ui/views/bubble/bubble_anchor.h"

class MediaToolbarButtonController;
class MediaToolbarButtonObserver;

// Base class for the Global Media Controls toolbar button.
// Allows callers to interact with and observe the media icon without needing
// to depend on the concrete Views or WebUI implementations.
class MediaToolbarButton {
 public:
  MediaToolbarButton();
  MediaToolbarButton(const MediaToolbarButton&) = delete;
  MediaToolbarButton& operator=(const MediaToolbarButton&) = delete;
  virtual ~MediaToolbarButton();

  void AddObserver(MediaToolbarButtonObserver* observer);
  void RemoveObserver(MediaToolbarButtonObserver* observer);

  // Returns whether the media button should currently be visible in the
  // toolbar.
  virtual bool IsVisible() const = 0;

  // Returns the bubble anchor to be used when positioning Global Media Control
  // dialogs or prompts relative to the media button.
  virtual views::BubbleAnchor GetBubbleAnchor() = 0;

  // Returns the controller responsible for managing the visibility and state
  // of the media button in response to media notifications and sessions.
  virtual MediaToolbarButtonController* GetController() = 0;

 protected:
  void NotifyMediaDialogOpened();
  void NotifyMediaButtonShown();
  void NotifyMediaButtonHidden();
  void NotifyMediaButtonEnabled();
  void NotifyMediaButtonDisabled();

 private:
  base::ObserverList<MediaToolbarButtonObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_H_
