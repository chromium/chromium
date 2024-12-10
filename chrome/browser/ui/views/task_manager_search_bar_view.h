// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_SEARCH_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_SEARCH_BAR_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace task_manager {
class TaskManagerSearchBarView : public views::View,
                                 public views::TextfieldController {
  METADATA_HEADER(TaskManagerSearchBarView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kInputField);

  TaskManagerSearchBarView(const std::u16string& placeholder,
                           const gfx::Insets& margins);
  TaskManagerSearchBarView(const TaskManagerSearchBarView&) = delete;
  TaskManagerSearchBarView& operator=(const TaskManagerSearchBarView&) = delete;
  ~TaskManagerSearchBarView() override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Focus on the input field.
  void Focus();
  bool GetClearButtonVisibleStatusForTesting() const;
  void SetInputTextForTesting(const std::u16string& text);
  gfx::Point GetClearButtonScreenCenterPointForTesting() const;

 private:
  void OnClearPressed();

  raw_ptr<views::Textfield> input_ = nullptr;
  raw_ptr<views::Button> clear_ = nullptr;
};
}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_SEARCH_BAR_VIEW_H_
