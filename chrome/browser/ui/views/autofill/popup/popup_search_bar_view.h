// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class Button;
class Textfield;
}  // namespace views

namespace autofill {

// This view enables users to filter popup suggestions. It contains
// the necessary elements for user input (text field, controls) and offers
// an API that allows the hosting popup to retrieve search queries and receive
// input event notifications.
class PopupSearchBarView : public views::View,
                           public views::FocusChangeListener,
                           public views::TextfieldController {
  METADATA_HEADER(PopupSearchBarView, views::View)

 public:
  using OnInputChangedCallback =
      base::RepeatingCallback<void(const std::u16string&)>;

  class Delegate {
   public:
    // Called when text in the textfield changes. Calls are throttled with
    // a delay of kInputChangeCallbackDelay to avoid excessive triggering.
    virtual void SearchBarOnInputChanged(const std::u16string& text) = 0;

    // Called when the controls (textfield and clear button) lose focus.
    virtual void SearchBarOnFocusLost() = 0;

    // Keyboard events from the textfield are passed to this method first.
    // The delegate returns `true` if the event was handled, this suppresses
    // the default behaviour in the textfield. As an example, the LEFT/RIGHT
    // arrow keys handled will not change the position of the text cursor.
    virtual bool SearchBarHandleKeyPressed(const ui::KeyEvent& event) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kInputField);

  // The delay between the textfield text change and triggering
  // the `OnInputChangedCallback`, used to throttle fast user input.
  static constexpr base::TimeDelta kInputChangeCallbackDelay =
      base::Milliseconds(250);

  PopupSearchBarView(const std::u16string& placeholder, Delegate& delegate);
  PopupSearchBarView(const PopupSearchBarView&) = delete;
  PopupSearchBarView& operator=(const PopupSearchBarView&) = delete;
  ~PopupSearchBarView() override;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override {}
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Focuses on the input field.
  void Focus();

  void SetInputTextForTesting(const std::u16string& text);
  gfx::Point GetClearButtonScreenCenterPointForTesting() const;

  // TODO(crbug.com/325246516): Add methods to support communication with its
  // hosting poopup view.

 private:
  void OnInputChanged();
  void OnClearPressed();

  const raw_ref<Delegate> delegate_;

  raw_ptr<views::Textfield> input_ = nullptr;
  raw_ptr<views::Button> clear_ = nullptr;

  base::CallbackListSubscription input_changed_subscription_;
  base::OneShotTimer input_change_notification_timer_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_
