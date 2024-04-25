// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "ui/base/interaction/element_identifier.h"
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
                           public views::FocusChangeListener {
  METADATA_HEADER(PopupSearchBarView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kInputField);

  PopupSearchBarView(const std::u16string& placeholder,
                     base::RepeatingClosure on_focus_lost_callback);
  PopupSearchBarView(const PopupSearchBarView&) = delete;
  PopupSearchBarView& operator=(const PopupSearchBarView&) = delete;
  ~PopupSearchBarView() override;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override {}
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // Focuses on the input field.
  void Focus();

  // TODO(b/325246516): Add methods to support communication with its hosting
  // poopup view.

 private:
  raw_ptr<views::Textfield> input_ = nullptr;
  raw_ptr<views::Button> clear_ = nullptr;
  base::RepeatingClosure on_focus_lost_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_
