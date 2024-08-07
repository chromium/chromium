// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_TITLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_TITLE_VIEW_H_

#include <string>

#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace autofill {

class PopupTitleView : public views::View {
  METADATA_HEADER(PopupTitleView, views::View)

 public:
  explicit PopupTitleView(std::u16string_view title);

  PopupTitleView(const PopupTitleView&) = delete;
  PopupTitleView& operator=(const PopupTitleView&) = delete;

  ~PopupTitleView() override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_TITLE_VIEW_H_
