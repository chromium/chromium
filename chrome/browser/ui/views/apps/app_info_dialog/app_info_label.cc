// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_label.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/focus_ring.h"

AppInfoLabel::AppInfoLabel(const std::u16string& text)
    : AppInfoLabel(text,
                   views::style::CONTEXT_LABEL,
                   views::style::STYLE_PRIMARY) {}

AppInfoLabel::~AppInfoLabel() = default;

AppInfoLabel::AppInfoLabel(const std::u16string& text,
                           int text_context,
                           int text_style,
                           gfx::DirectionalityMode directionality_mode)
    : Label(text, text_context, text_style, directionality_mode) {
  // Note that ACCESSIBLE_ONLY only works in AccessiblePaneView, which these
  // labels are not part of. So we need to mark them as ALWAYS because the user
  // still needs to be able to tab-navigate them and get screen reader feedback.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  views::FocusRing::Install(this);
}

BEGIN_METADATA(AppInfoLabel)
END_METADATA
