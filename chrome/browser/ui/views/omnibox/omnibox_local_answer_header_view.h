// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_LOCAL_ANSWER_HEADER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_LOCAL_ANSWER_HEADER_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Throbber;
class Label;
}  // namespace views

class OmniboxLocalAnswerHeaderView : public views::View {
  METADATA_HEADER(OmniboxLocalAnswerHeaderView, views::View)

 public:
  OmniboxLocalAnswerHeaderView();

  // views::View:
  void OnThemeChanged() override;

  // Toggle visibility between `throbber_` and `icon_`.
  void SetThrobberVisibility(bool visible);

  void SetText(const std::u16string& text);

 private:
  raw_ptr<views::Throbber> throbber_;
  raw_ptr<views::ImageView> icon_;
  raw_ptr<views::Label> text_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_LOCAL_ANSWER_HEADER_VIEW_H_
