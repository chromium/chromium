// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_label.h"

#include "ui/gfx/canvas.h"

AppInfoLabel::AppInfoLabel(const base::string16& text)
    : AppInfoLabel(text,
                   views::style::CONTEXT_LABEL,
                   views::style::STYLE_PRIMARY) {}

AppInfoLabel::AppInfoLabel(const base::string16& text,
                           int text_context,
                           int text_style,
                           gfx::DirectionalityMode directionality_mode)
    : Label(text, text_context, text_style, directionality_mode) {
  // Note that ACCESSIBLE_ONLY only works in AccessiblePaneView, which these
  // labels are not part of. So we need to mark them as ALWAYS because the user
  // still needs to be able to tab-navigate them and get screen reader feedback.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void AppInfoLabel::PaintFocusRing(gfx::Canvas* canvas) const {
  gfx::Rect focus_ring_bounds = GetTextBounds();
  focus_ring_bounds.Intersect(GetLocalBounds());
  canvas->DrawFocusRect(focus_ring_bounds);
}

void AppInfoLabel::OnFocus() {
  Label::OnFocus();
  SchedulePaint();
}

void AppInfoLabel::OnBlur() {
  Label::OnBlur();
  SchedulePaint();
}
