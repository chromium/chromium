// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/caption_button_placeholder_container.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"

CaptionButtonPlaceholderContainer::CaptionButtonPlaceholderContainer() {
  SetPaintToLayer();
}

CaptionButtonPlaceholderContainer::~CaptionButtonPlaceholderContainer() =
    default;

BEGIN_METADATA(CaptionButtonPlaceholderContainer)
END_METADATA
