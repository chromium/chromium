// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RECORD_REPLAY_REPLAY_RECORDING_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_RECORD_REPLAY_REPLAY_RECORDING_BUBBLE_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/record_replay/core/browser/recording.pb.h"

namespace content {
class WebContents;
}

namespace views {
class Widget;
}  // namespace views

namespace record_replay {

class RecordReplayManager;

// The bubble view for confirming replaying a recording.
// Shows the name of the recording.
class ReplayRecordingBubbleView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(ReplayRecordingBubbleView, LocationBarBubbleDelegateView)

 public:
  // Creates and shows the bubble.
  // `anchor` is the view to anchor the bubble to (e.g. page action icon).
  // `recordings` is the list of recordings to show. `manager` is the record
  // replay manager.
  static std::unique_ptr<views::Widget> Show(
      views::BubbleAnchor anchor,
      content::WebContents* web_contents,
      std::vector<record_replay::Recording> recordings,
      base::WeakPtr<RecordReplayManager> manager);

  ReplayRecordingBubbleView(views::BubbleAnchor anchor,
                            content::WebContents* web_contents,
                            std::vector<record_replay::Recording> recordings,
                            base::WeakPtr<RecordReplayManager> manager);
  ~ReplayRecordingBubbleView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  std::u16string GetWindowTitle() const override;

  void SetReplayCallbackForTesting(base::RepeatingClosure callback);  // IN-TEST

  void OnReplayButtonClicked(size_t index);

 private:
  void AddRecentTasks();
  void OnRecordButtonClicked();

  std::vector<record_replay::Recording> recordings_;
  base::WeakPtr<RecordReplayManager> manager_;
  base::RepeatingClosure replay_callback_for_testing_;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_UI_VIEWS_RECORD_REPLAY_REPLAY_RECORDING_BUBBLE_VIEW_H_
