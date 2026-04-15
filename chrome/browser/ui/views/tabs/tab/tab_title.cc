// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab/tab_title.h"

#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "ui/base/metadata/metadata_impl_macros.h"

TabTitle::TabTitle() {
  SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  SetElideBehavior(gfx::FADE_TAIL);
  SetHandlesTooltips(false);
  SetAutoColorReadabilityEnabled(false);
  SetText(CoreTabHelper::GetDefaultTitle());
  SetBackgroundColor(SK_ColorTRANSPARENT);
  // Title paints on top of an opaque region (the tab background) of a
  // non-opaque layer (the tabstrip's layer), which cannot currently be detected
  // by the subpixel-rendering opacity check.
  SetSkipSubpixelRenderingOpacityCheck(true);
  SetCanProcessEventsWithinSubtree(false);
  SetCollapseWhenHidden(true);
}

BEGIN_METADATA(TabTitle)
END_METADATA
