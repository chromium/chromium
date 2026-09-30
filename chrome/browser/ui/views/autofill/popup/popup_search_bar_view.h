// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class Button;
class Textfield;
class Throbber;
class ImageView;
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
    // Called when text in the textfield changes. Calls are throttled by the
    // configured `debounce_delay_` (defaults to `kInputChangeCallbackDelay`) to
    // avoid excessive triggering.
    virtual void SearchBarOnInputChanged(std::u16string_view text) = 0;

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

  // The default delay between a textfield change and triggering
  // `Delegate::SearchBarOnInputChanged()`, used to throttle fast user input.
  static constexpr base::TimeDelta kInputChangeCallbackDelay =
      base::Milliseconds(250);

  // How long each placeholder is shown while rotating placeholders.
  static constexpr base::TimeDelta kPlaceholderRotationInterval =
      base::Seconds(3);

  // Calls to `Delegate::SearchBarOnInputChanged()` are throttled by
  // `debounce_delay`.
  // If `rotating_placeholders` is not empty, `placeholder` is only shown for
  // `kPlaceholderRotationInterval` after the input field is (or becomes)
  // empty. Then, the placeholder cycles through `rotating_placeholders`, in
  // order and starting at a random one, every `kPlaceholderRotationInterval`
  // until text is entered.
  PopupSearchBarView(
      const std::u16string& placeholder,
      const std::u16string& initial_value,
      std::vector<std::u16string> rotating_placeholders,
      Delegate& delegate,
      bool show_search_icon_sparkle = false,
      base::TimeDelta debounce_delay = kInputChangeCallbackDelay);
  PopupSearchBarView(const PopupSearchBarView&) = delete;
  PopupSearchBarView& operator=(const PopupSearchBarView&) = delete;
  ~PopupSearchBarView() override;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::FocusChangeListener:
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Focuses on the input field.
  void Focus();

  // Returns the current text in the input field.
  std::u16string GetText() const;

  // Sets the loading state of the search bar, showing a throbber if loading.
  void SetLoading(bool is_loading);

  // Handles key press events from child views (input field and clear button).
  bool HandleKeyPressed(views::View* sender, const ui::KeyEvent& event);

  void SetInputTextForTesting(const std::u16string& text);
  std::u16string_view GetPlaceholderTextForTesting() const;
  gfx::Point GetClearButtonScreenCenterPointForTesting() const;
  bool IsClearButtonVisibleForTesting() const;
  views::ImageView* GetSearchIconForTesting() const { return search_icon_; }
  views::Throbber* GetThrobberForTesting() const { return throbber_; }

  // TODO(crbug.com/325246516): Add methods to support communication with its
  // hosting poopup view.

 private:
  void OnInputChanged();
  void OnClearPressed();

  // Starts the placeholder rotation. No-op if `rotating_placeholders_` is
  // empty or the rotation is already running.
  void StartPlaceholderRotation();

  // Restores `placeholder_` and stops the placeholder rotation. No-op if the
  // rotation is not running.
  void StopPlaceholderRotation();

  const raw_ref<Delegate> delegate_;

  raw_ptr<views::Textfield> input_ = nullptr;
  raw_ptr<views::Button> clear_ = nullptr;
  raw_ptr<views::ImageView> search_icon_ = nullptr;
  raw_ptr<views::Throbber> throbber_ = nullptr;

  base::CallbackListSubscription input_changed_subscription_;
  base::OneShotTimer input_change_notification_timer_;
  const base::TimeDelta debounce_delay_;

  // The default placeholder. It is shown whenever the input field becomes
  // empty and, if `rotating_placeholders_` is not empty, only until the first
  // rotation. It is kept separate from `rotating_placeholders_` because it is
  // not part of the rotation loop.
  const std::u16string placeholder_;
  // Placeholders that are cycled through, in order and starting at a random
  // one, while the input field is empty. May be empty, in which case
  // `placeholder_` is always shown.
  const std::vector<std::u16string> rotating_placeholders_;
  // Index of the next placeholder to show from `rotating_placeholders_`.
  size_t next_rotating_placeholder_index_ = 0;
  base::RepeatingTimer placeholder_rotation_timer_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_
