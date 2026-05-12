// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RECORD_REPLAY_REPLAY_RECORDING_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_RECORD_REPLAY_REPLAY_RECORDING_BUBBLE_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

namespace views {
class Widget;
}  // namespace views

namespace record_replay {

// The bubble view for confirming replaying a recording.
// Shows the name of the recording.
class ReplayRecordingBubbleView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(ReplayRecordingBubbleView, LocationBarBubbleDelegateView)

 public:
  // Creates and shows the bubble.
  // `anchor_view` is the view to anchor the bubble to (e.g. page action icon).
  // `recording_name` is the name of the recording to show.
  // `on_replay_started` is called when the user confirms replay.
  static std::unique_ptr<views::Widget> Show(
      views::BubbleAnchor anchor,
      content::WebContents* web_contents,
      const std::u16string& recording_name,
      base::OnceClosure on_replay_started);

  ReplayRecordingBubbleView(views::BubbleAnchor anchor,
                            content::WebContents* web_contents,
                            const std::u16string& recording_name,
                            base::OnceClosure on_replay_started);
  ~ReplayRecordingBubbleView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  std::u16string GetWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;

 private:
  std::u16string recording_name_;
  base::OnceClosure on_replay_started_;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_UI_VIEWS_RECORD_REPLAY_REPLAY_RECORDING_BUBBLE_VIEW_H_
