// Copyright 2023 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_RECORD_REPLAY_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_RECORD_REPLAY_TOOLBAR_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "content/public/browser/web_contents.h"

class Browser;

struct RecordReplayToolbarButtonWebContentsObserver;

class RecordReplayToolbarButton: public ToolbarButton {
 friend struct RecordReplayToolbarButtonWebContentsObserver;
 public:
  explicit RecordReplayToolbarButton(Browser* browser);
  RecordReplayToolbarButton(const RecordReplayToolbarButton&) = delete;
  RecordReplayToolbarButton& operator=(const RecordReplayToolbarButton&) = delete;
  ~RecordReplayToolbarButton() override;

  void OnPaintBackground(gfx::Canvas* canvas) override;
 private:
  void CreateHiddenWebContents();
  void ButtonPressed();
  void StartRecording();
  void StopRecording();
  void RecordingTabDestroyed();

  void RefreshIconState();
  void CreatePostRecordingWebContents();

  const raw_ptr<Browser> browser_;
  content::WebContents* web_contents_;
  std::unique_ptr<RecordReplayToolbarButtonWebContentsObserver>
    web_contents_observer_;
  // our hidden webcontent running business/auth logic.
  std::unique_ptr<content::WebContents> recordreplay_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_RECORD_REPLAY_TOOLBAR_BUTTON_H_
