// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_VOICE_VOICE_PLATE_H_
#define CHROME_BROWSER_UI_VIEWS_VOICE_VOICE_PLATE_H_

#include <memory>

#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace voice {

// The shell of a voice plate: the toast-like bubble, centered at the top of
// the browser window, that hosts the main UI surface of a voice experience.
class VoicePlate : public views::BubbleDialogDelegate {
 public:
  VoicePlate(const VoicePlate&) = delete;
  VoicePlate& operator=(const VoicePlate&) = delete;
  ~VoicePlate() override;

  virtual void Show();

  // views::BubbleDialogDelegate:
  gfx::Rect GetBubbleBounds() override;
  void Init() override;

 protected:
  explicit VoicePlate(views::View* anchor_view);

  // Call at the end of the subclass constructor, after `SetContentsView()`.
  void CreateWidget();

 private:
  std::unique_ptr<views::Widget> widget_;
};

}  // namespace voice

#endif  // CHROME_BROWSER_UI_VIEWS_VOICE_VOICE_PLATE_H_
