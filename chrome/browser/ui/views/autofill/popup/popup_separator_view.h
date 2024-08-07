// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEPARATOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEPARATOR_VIEW_H_

#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace autofill {

// Draws a separator between sections of the dropdown, namely between datalist
// (https://developer.mozilla.org/en-US/docs/Web/HTML/Element/datalist) and
// Autofill suggestions. Note that this is NOT the same as the border on top of
// the footer section or the border between footer items.
class PopupSeparatorView : public views::View {
  METADATA_HEADER(PopupSeparatorView, views::View)

 public:
  explicit PopupSeparatorView(int vertical_padding);
  ~PopupSeparatorView() override;

  PopupSeparatorView(const PopupSeparatorView&) = delete;
  PopupSeparatorView& operator=(const PopupSeparatorView&) = delete;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_SEPARATOR_VIEW_H_
