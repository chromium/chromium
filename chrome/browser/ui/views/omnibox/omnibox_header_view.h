// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_HEADER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_HEADER_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/omnibox/omnibox_mouse_enter_exit_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

class OmniboxPopupViewViews;

namespace gfx {
class Insets;
}  // namespace gfx

namespace views {
class Label;
}  // namespace views

namespace ui {
class MouseEvent;
}  // namespace ui

class OmniboxHeaderView : public views::View {
  METADATA_HEADER(OmniboxHeaderView, views::View)

 public:
  explicit OmniboxHeaderView(OmniboxPopupViewViews* popup_view);

  void SetHeader(const std::u16string& header_text);

  // views::View:
  gfx::Insets GetInsets() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;

 private:
  // The parent view.
  const raw_ptr<OmniboxPopupViewViews> popup_view_;

  // The Label containing the header text. This is never nullptr.
  raw_ptr<views::Label> header_label_;

  // The unmodified header text for this header.
  std::u16string header_text_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_HEADER_VIEW_H_
