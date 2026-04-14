// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_INFO_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_INFO_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace user_education {

// Object that holds a reference to a help bubble view and its widget.
struct HelpBubbleViewInfo {
  HelpBubbleViewInfo();
  HelpBubbleViewInfo(std::unique_ptr<views::Widget> widget_,
                     views::BubbleDialogDelegateView* bubble_view_);
  HelpBubbleViewInfo(HelpBubbleViewInfo&&) noexcept;
  HelpBubbleViewInfo& operator=(HelpBubbleViewInfo&&) noexcept;
  ~HelpBubbleViewInfo();

  std::unique_ptr<views::Widget> widget;
  raw_ptr<views::BubbleDialogDelegateView> bubble_view = nullptr;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_INFO_H_
