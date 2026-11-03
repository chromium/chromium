// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/user_education_types.h"

#include "ui/base/mojom/window_open_disposition.mojom-shared.h"

namespace user_education {
ui::mojom::WindowOpenDisposition GetWindowOpenDisposition(
    PageOpenMode page_open_mode) {
  switch (page_open_mode) {
    case PageOpenMode::kOverwriteActiveTab:
      return ui::mojom::WindowOpenDisposition::kCurrentTab;
    case PageOpenMode::kNewForegroundTab:
      return ui::mojom::WindowOpenDisposition::kNewForegroundTab;
    case PageOpenMode::kSingletonTab:
      return ui::mojom::WindowOpenDisposition::kSingletonTab;
  }
}
}  // namespace user_education
