// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_content_previews.h"

#include <utility>

SharesheetContentPreviews::SharesheetContentPreviews(
    apps::mojom::IntentPtr intent) {
  intent_ = std::move(intent);
}

SharesheetContentPreviews::~SharesheetContentPreviews() = default;

void SharesheetContentPreviews::SetTitlePreview() {
  // TODO(crbug.com/2631548): Add code to add a new
  // label view to reflect file name to the
  // content previews view.
  NOTIMPLEMENTED();
}

void SharesheetContentPreviews::SetImagePreview() {
  // TODO(crbug.com/2631548): Add code to add a new
  // image view to show image preview to the
  // content previews view.
  NOTIMPLEMENTED();
}
