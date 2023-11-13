// Copyright 2023 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_RECORD_REPLAY_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_RECORD_REPLAY_TOOLBAR_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class Browser;

class RecordReplayToolbarButton : public ToolbarButton {
 public:
  explicit RecordReplayToolbarButton(Browser* browser);
  RecordReplayToolbarButton(const RecordReplayToolbarButton&) = delete;
  RecordReplayToolbarButton& operator=(const RecordReplayToolbarButton&) = delete;
  ~RecordReplayToolbarButton() override;

 private:
  void ButtonPressed();

  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_RECORD_REPLAY_TOOLBAR_BUTTON_H_
