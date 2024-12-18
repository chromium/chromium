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
  using OnInputChangedCallback =
      base::RepeatingCallback<void(const std::u16string&)>;

  class Delegate {
   public:
    // Called when text in the textfield changes. Calls are throttled with
    // a delay of kInputChangeCallbackDelay to avoid excessive triggering.
    virtual void SearchBarOnInputChanged(const std::u16string& text) = 0;
    // Called when the controls (textfield and clear button) hover status
    // changes.
    virtual void SearchBarOnHoverChange(const bool is_focus_on) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kInputField);

  // The delay between the textfield text change and triggering
  // the `OnInputChangedCallback`, used to throttle fast user input.
  static constexpr base::TimeDelta kInputChangeCallbackDelay =
      base::Milliseconds(50);

  TaskManagerSearchBarView(const std::u16string& placeholder,
                           const gfx::Insets& margins,
                           Delegate& delegate);
  TaskManagerSearchBarView(const TaskManagerSearchBarView&) = delete;
  TaskManagerSearchBarView& operator=(const TaskManagerSearchBarView&) = delete;
  ~TaskManagerSearchBarView() override;

  // views::View:
  void OnThemeChanged() override;

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Focus on the input field.
  void Focus();
  bool GetClearButtonVisibleStatusForTesting() const;
  void SetInputTextForTesting(const std::u16string& text);
  gfx::Point GetClearButtonScreenCenterPointForTesting() const;

  // Updates related fields on the Textfield.
  void UpdateTextfield();

 private:
  void OnInputChanged();
  void OnClearPressed();

  const raw_ref<Delegate> delegate_;

  // Textfield placeholder color.
  std::optional<ui::ColorId> textfield_placeholder_color_id_;

  raw_ptr<views::Textfield> input_ = nullptr;
  raw_ptr<views::Button> clear_ = nullptr;

  base::CallbackListSubscription input_changed_subscription_;
  base::OneShotTimer input_change_notification_timer_;
};
}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_SEARCH_BAR_VIEW_H_
