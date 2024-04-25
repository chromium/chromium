// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_

#include <string>

#include "ui/base/interaction/element_identifier.h"
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
class PopupSearchBarView : public views::View {
  METADATA_HEADER(PopupSearchBarView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kInputField);

  explicit PopupSearchBarView(const std::u16string& placeholder);
  PopupSearchBarView(const PopupSearchBarView&) = delete;
  PopupSearchBarView& operator=(const PopupSearchBarView&) = delete;
  ~PopupSearchBarView() override;

  // Focuses on the input field.
  void Focus();

  // TODO(b/325246516): Add methods to support communication with its hosting
  // poopup view.

 private:
  raw_ptr<views::Textfield> input_ = nullptr;
  raw_ptr<views::Button> clear_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEARCH_BAR_VIEW_H_
