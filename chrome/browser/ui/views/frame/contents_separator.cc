// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_separator.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"

ContentsSeparator::ContentsSeparator() {
  SetBackground(
      views::CreateSolidBackground(kColorToolbarContentAreaSeparator));

  // BrowserViewLayout will respect either the height or width of this,
  // depending on orientation, not simultaneously both.
  SetPreferredSize(
      gfx::Size(views::Separator::kThickness, views::Separator::kThickness));
}

BEGIN_METADATA(ContentsSeparator)
END_METADATA
