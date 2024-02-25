// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_UTIL_H_

#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/table_layout_view.h"

namespace sharing_hub {

// Defines a title view with a back button and a label. Used by first party
// action dialogs for the sharing hub (e.g. qr code, send tab to self).
class TitleWithBackButtonView : public views::TableLayoutView {
  METADATA_HEADER(TitleWithBackButtonView, views::TableLayoutView)

 public:
  explicit TitleWithBackButtonView(views::Button::PressedCallback callback,
                                   const std::u16string& window_title);
  ~TitleWithBackButtonView() override;

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override;
};

BEGIN_VIEW_BUILDER(, TitleWithBackButtonView, views::TableLayoutView)
END_VIEW_BUILDER

}  // namespace sharing_hub

DEFINE_VIEW_BUILDER(, sharing_hub::TitleWithBackButtonView)

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_UTIL_H_
