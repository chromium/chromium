// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CENTERED_TEXT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CENTERED_TEXT_VIEW_H_

#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace autofill {

// A simple, non-interactive view that displays a centered text message.
// It has a `STYLE_BODY_4` style and a static height of 48px.
class PopupCenteredTextView : public views::View {
  METADATA_HEADER(PopupCenteredTextView, views::View)

 public:
  explicit PopupCenteredTextView(const std::u16string& message);

  PopupCenteredTextView(const PopupCenteredTextView&) = delete;
  PopupCenteredTextView& operator=(const PopupCenteredTextView&) = delete;

  ~PopupCenteredTextView() override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CENTERED_TEXT_VIEW_H_
