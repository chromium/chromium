// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

NonAccessibleImageView::NonAccessibleImageView() {
  GetViewAccessibility().SetIsInvisible(true);
}

NonAccessibleImageView::~NonAccessibleImageView() {}


BEGIN_METADATA(NonAccessibleImageView)
END_METADATA
