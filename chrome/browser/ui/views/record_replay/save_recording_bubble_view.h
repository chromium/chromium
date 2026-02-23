// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RECORD_REPLAY_SAVE_RECORDING_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_RECORD_REPLAY_SAVE_RECORDING_BUBBLE_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/record_replay/save_recording_bubble_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace content {
class WebContents;
}

namespace views {
class Textfield;
class View;
}  // namespace views

namespace record_replay {

// The bubble view for saving a recording.
// Allows the user to enter a name for the recording.
class SaveRecordingBubbleView : public LocationBarBubbleDelegateView,
                                public views::TextfieldController {
 public:
  METADATA_HEADER(SaveRecordingBubbleView, LocationBarBubbleDelegateView)

 public:
  // Creates and shows the bubble.
  // |anchor_view| is the view to anchor the bubble to (e.g. page action icon).
  // |controller| handles the logic. The view takes ownership of the controller.
  static void Show(views::View* anchor_view,
                   content::WebContents* web_contents,
                   std::unique_ptr<SaveRecordingBubbleController> controller);

  // Use Show() to create and show.
  SaveRecordingBubbleView(
      views::View* anchor_view,
      content::WebContents* web_contents,
      std::unique_ptr<SaveRecordingBubbleController> controller);
  ~SaveRecordingBubbleView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  std::u16string GetWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

 private:
  std::unique_ptr<SaveRecordingBubbleController> controller_;
  raw_ptr<views::Textfield> name_textfield_ = nullptr;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_UI_VIEWS_RECORD_REPLAY_SAVE_RECORDING_BUBBLE_VIEW_H_
