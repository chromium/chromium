// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_PRE_TARGET_HANDLER_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_PRE_TARGET_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/views/view.h"

namespace quick_answers {

class RichAnswersView;

// This class handles mouse events, such as dismissal of the
// rich answers view.
class RichAnswersPreTargetHandler : public ui::EventHandler {
 public:
  explicit RichAnswersPreTargetHandler(RichAnswersView* view);

  // Disallow copy and assign.
  RichAnswersPreTargetHandler(const RichAnswersPreTargetHandler&) = delete;
  RichAnswersPreTargetHandler& operator=(const RichAnswersPreTargetHandler&) =
      delete;

  ~RichAnswersPreTargetHandler() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* key_event) override;
  void OnMouseEvent(ui::MouseEvent* mouse_event) override;
  void OnScrollEvent(ui::ScrollEvent* scroll_event) override;

 private:
  // Associated view handled by this class.
  const raw_ptr<views::View> view_;
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_PRE_TARGET_HANDLER_H_
