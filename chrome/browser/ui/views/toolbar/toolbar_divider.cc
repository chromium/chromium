// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_divider.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"

ToolbarDivider::ToolbarDivider() {
  SetPreferredSize(
      gfx::Size(GetLayoutConstant(LayoutConstant::kToolbarDividerWidth),
                GetLayoutConstant(LayoutConstant::kToolbarDividerHeight)));
  SetBackground(views::CreateRoundedRectBackground(
      kColorToolbarExtensionSeparatorEnabled,
      GetLayoutConstant(LayoutConstant::kToolbarDividerCornerRadius)));
}

ToolbarDivider::~ToolbarDivider() = default;

BEGIN_METADATA(ToolbarDivider)
END_METADATA
