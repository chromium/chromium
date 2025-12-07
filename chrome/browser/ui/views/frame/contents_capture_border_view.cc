// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_capture_border_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

ContentsCaptureBorderView::ContentsCaptureBorderView() {
  SetProperty(views::kElementIdentifierKey, kContentsCaptureBorder);
  SetBorder(views::CreateSolidBorder(kContentsBorderThickness,
                                     kColorCapturedTabContentsBorder));
}

ContentsCaptureBorderView::~ContentsCaptureBorderView() = default;

BEGIN_METADATA(ContentsCaptureBorderView)
END_METADATA
