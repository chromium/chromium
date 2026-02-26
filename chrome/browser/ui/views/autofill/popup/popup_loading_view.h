// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_LOADING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_LOADING_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace autofill {

// A view that displays a loading throbber centered within a calculated height.
class PopupLoadingView : public views::View {
  METADATA_HEADER(PopupLoadingView, views::View)

 public:
  explicit PopupLoadingView(int expected_number_of_suggestions);
  PopupLoadingView(const PopupLoadingView&) = delete;
  PopupLoadingView& operator=(const PopupLoadingView&) = delete;
  ~PopupLoadingView() override = default;

 private:
  // Calculates the preferred size based on the expected number of suggestions.
  gfx::Size CalculateSizeOfSuggestions(
      int expected_number_of_suggestions) const;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_LOADING_VIEW_H_
