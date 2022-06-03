// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/try_chrome_dialog_win/button_layout.h"

#include "base/check_op.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

ButtonLayout::ButtonLayout(int view_width) : view_width_(view_width) {}

ButtonLayout::~ButtonLayout() = default;

void ButtonLayout::Layout(views::View* host) {
  const gfx::Size& host_size = host->bounds().size();

  // Layout of the host within its parent must size it based on the |view_width|
  // given to this layout manager at creation. If it happens to be different,
  // the buttons will be sized and positioned based on the host's true size.
  // This will result in either stretching or compressing the buttons, and may
  // lead to elision of their text.
  DCHECK_EQ(host_size.width(), view_width_);

  // The buttons are all equal-sized.
  const gfx::Size max_child_size = GetMaxChildPreferredSize(host);
  gfx::Size button_size(host_size.width(), max_child_size.height());
  const auto& children = host->children();
  if (UseWideButtons(host_size.width(), max_child_size.width())) {
    children[0]->SetBoundsRect({gfx::Point(), button_size});
    if (children.size() > 1) {
      int bottom_y = button_size.height() + kPaddingBetweenButtons;
      children[1]->SetBoundsRect({{0, bottom_y}, button_size});
    }
  } else {
    button_size.set_width((host_size.width() - kPaddingBetweenButtons) / 2);
    // The offset of the right-side narrow button.
    const int right_x = button_size.width() + kPaddingBetweenButtons;
    auto right_button = children.begin();
    if (children.size() > 1) {
      children[0]->SetBoundsRect({gfx::Point(), button_size});
      ++right_button;
    }
    (*right_button)->SetBoundsRect({{right_x, 0}, button_size});
  }
}

gfx::Size ButtonLayout::GetPreferredSize(const views::View* host) const {
  const gfx::Size max_child_size = GetMaxChildPreferredSize(host);

  // |view_width_| is a hard limit; the buttons will be sized and positioned to
  // fill it.
  if ((host->children().size() > 1) &&
      UseWideButtons(view_width_, max_child_size.width())) {
    // Two rows of equal height with padding between them.
    return {view_width_, max_child_size.height() * 2 + kPaddingBetweenButtons};
  }

  // Only one button or the widest of two is sufficiently narrow, so only one
  // row is needed.
  return {view_width_, max_child_size.height()};
}

// static
gfx::Size ButtonLayout::GetMaxChildPreferredSize(const views::View* host) {
  const auto& children = host->children();
  gfx::Size max_child_size = children[0]->GetPreferredSize();
  if (children.size() > 1)
    max_child_size.SetToMax(children[1]->GetPreferredSize());
  return max_child_size;
}

// static
bool ButtonLayout::UseWideButtons(int host_width, int max_child_width) {
  return max_child_width > (host_width - kPaddingBetweenButtons) / 2;
}
