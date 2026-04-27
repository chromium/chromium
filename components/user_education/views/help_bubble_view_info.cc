// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_view_info.h"

namespace user_education {

HelpBubbleViewInfo::HelpBubbleViewInfo(
    std::unique_ptr<views::Widget> widget_,
    views::BubbleDialogDelegateView* bubble_view_)
    : widget(std::move(widget_)), bubble_view(bubble_view_) {}

HelpBubbleViewInfo::HelpBubbleViewInfo() = default;
HelpBubbleViewInfo::HelpBubbleViewInfo(HelpBubbleViewInfo&&) noexcept = default;
HelpBubbleViewInfo& HelpBubbleViewInfo::operator=(
    HelpBubbleViewInfo&&) noexcept = default;
HelpBubbleViewInfo::~HelpBubbleViewInfo() = default;

}  // namespace user_education
