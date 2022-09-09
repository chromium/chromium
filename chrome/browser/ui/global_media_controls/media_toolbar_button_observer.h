// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_OBSERVER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_OBSERVER_H_

#include "base/observer_list_types.h"

class MediaToolbarButtonObserver : public base::CheckedObserver {
 public:
  // Called when the media dialog is opened.
  virtual void OnMediaDialogOpened() = 0;

  // Called when the toolbar button is shown.
  virtual void OnMediaButtonShown() = 0;

  // Called when the toolbar button is hidden.
  virtual void OnMediaButtonHidden() = 0;

  // Called when the toolbar button is enabled.
  virtual void OnMediaButtonEnabled() = 0;

  // Called when the toolbar button is disabled.
  virtual void OnMediaButtonDisabled() = 0;

 protected:
  ~MediaToolbarButtonObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_OBSERVER_H_
