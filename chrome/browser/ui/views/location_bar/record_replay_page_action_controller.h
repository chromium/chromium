// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_RECORD_REPLAY_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_RECORD_REPLAY_PAGE_ACTION_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/record_replay/core/browser/record_replay_manager.h"
#include "components/record_replay/core/browser/recording.pb.h"
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

namespace views {
class Widget;
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

  bool has_recording_for_testing() const { return !recent_recordings_.empty(); }

 private:
  void UpdateState();
  void OnRetrieveRecordingsComplete(
      std::vector<record_replay::Recording> recordings);

  const raw_ref<tabs::TabInterface> tab_;
  const raw_ref<page_actions::PageActionController> page_action_controller_;
  base::RepeatingTimer timer_;

  std::vector<record_replay::Recording> recent_recordings_;

  std::unique_ptr<views::Widget> bubble_widget_;

  base::WeakPtrFactory<RecordReplayPageActionController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_RECORD_REPLAY_PAGE_ACTION_CONTROLLER_H_
