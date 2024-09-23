// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_NO_SUGGESTIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_NO_SUGGESTIONS_VIEW_H_

#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace autofill {

class PopupNoSuggestionsView : public views::View {
  METADATA_HEADER(PopupNoSuggestionsView, views::View)

 public:
  explicit PopupNoSuggestionsView(const std::u16string& message);

  PopupNoSuggestionsView(const PopupNoSuggestionsView&) = delete;
  PopupNoSuggestionsView& operator=(const PopupNoSuggestionsView&) = delete;

  ~PopupNoSuggestionsView() override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_NO_SUGGESTIONS_VIEW_H_
