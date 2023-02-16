// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_BUTTON_VIEW_H_

#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingButtonView
//
//  A helper class for buttons in the Read Anything toolbar.
//  This class makes image button views with padding and theming as a
//  convenience class for the ReadAnythingToolbarView.
//
class ReadAnythingButtonView : public views::View {
 public:
  METADATA_HEADER(ReadAnythingButtonView);
  ReadAnythingButtonView(const views::ImageButton::PressedCallback callback,
                         const gfx::VectorIcon& icon,
                         int icon_size,
                         SkColor icon_color,
                         const std::u16string& tooltip);
  ReadAnythingButtonView(const ReadAnythingButtonView&) = delete;
  ReadAnythingButtonView& operator=(const ReadAnythingButtonView&) = delete;
  ~ReadAnythingButtonView() override;

  void UpdateIcon(const gfx::VectorIcon& icon,
                  int icon_size,
                  SkColor icon_color);

 private:
  raw_ptr<views::ImageButton> button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_BUTTON_VIEW_H_
