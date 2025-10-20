// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_separator.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

// static
std::unique_ptr<ContentsSeparator>
ContentsSeparator::CreateLayerBasedContentsSeparator() {
  return base::WrapUnique<ContentsSeparator>(
      new ContentsSeparator(/*create_layer=*/true));
}

// static
std::unique_ptr<ContentsSeparator>
ContentsSeparator::CreateContentsSeparator() {
  return base::WrapUnique<ContentsSeparator>(
      new ContentsSeparator(/*create_layer=*/false));
}

ContentsSeparator::ContentsSeparator(bool create_layer) {
  SetBackground(create_layer ? views::CreateLayerBasedSolidBackground(
                                   kColorToolbarContentAreaSeparator)
                             : views::CreateSolidBackground(
                                   kColorToolbarContentAreaSeparator));

  // BrowserViewLayout will respect either the height or width of this,
  // depending on orientation, not simultaneously both.
  SetPreferredSize(
      gfx::Size(views::Separator::kThickness, views::Separator::kThickness));
}

BEGIN_METADATA(ContentsSeparator)
END_METADATA
