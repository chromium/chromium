// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_RECORD_REPLAY_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_RECORD_REPLAY_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/record_replay/record_replay_manager.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

namespace page_actions {
class PageActionController;
}

namespace actions {
class ActionItem;
}

// Controls the Record/Replay page action for a single tab.
//
// If a recording for the current tab exists, the action replays the recording
// or stops the replay.
// Otherwise, the action starts a new recording or stops it.
//
// The current implementation polls the RecordReplayManager to check if a
// recording for the current page exists.
//
// TODO(b/483386299): Replace polling with a more efficient method.
// TODO(b/483386299): Internationalize the strings.
class RecordReplayPageActionController {
 public:
  RecordReplayPageActionController(
      tabs::TabInterface& tab,
      page_actions::PageActionController& page_action_controller);
  ~RecordReplayPageActionController();

  DECLARE_USER_DATA(RecordReplayPageActionController);

  void ExecuteAction(actions::ActionItem* item);

 private:
  void UpdateState();
  void OnRetrieveRecordingComplete(std::optional<record_replay::Recording> r);

  const raw_ref<tabs::TabInterface> tab_;
  const raw_ref<page_actions::PageActionController> page_action_controller_;
  base::RepeatingTimer timer_;
  bool has_recording_ = false;
  base::WeakPtrFactory<RecordReplayPageActionController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_RECORD_REPLAY_PAGE_ACTION_CONTROLLER_H_
