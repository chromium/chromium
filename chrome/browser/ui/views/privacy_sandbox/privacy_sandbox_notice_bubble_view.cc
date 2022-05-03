// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_notice_bubble_view.h"

#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

// static
void ShowPrivacySandboxNoticeBubble(Browser* browser) {
  // TODO(crbug.com/1321587): Create BubbleDialogDelegate and show bubble.
}

PrivacySandboxNoticeBubbleView::PrivacySandboxNoticeBubbleView(Browser* browser)
    : browser_(browser) {
  // TODO(crbug.com/1321587): Implement view.
  SetUseDefaultFillLayout(true);
}

BEGIN_METADATA(PrivacySandboxNoticeBubbleView, views::View)
END_METADATA
