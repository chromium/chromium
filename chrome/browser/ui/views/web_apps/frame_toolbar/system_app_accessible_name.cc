// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/system_app_accessible_name.h"

#include <utility>

#include "chrome/browser/ui/views/chrome_typography.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"

SystemAppAccessibleName::SystemAppAccessibleName(const std::u16string& app_name)
    // TODO(crbug.com/40808413): Clean up the empty string (or remove this
    // class) after reaching a consensus with UX on button search behavior.
    : views::Label(u" ",
                   ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
                   views::style::STYLE_PRIMARY),
      app_name_(app_name) {
  SetEnabledColor(SK_ColorTRANSPARENT);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kApplication);
  GetViewAccessibility().SetName(app_name_);
}

SystemAppAccessibleName::~SystemAppAccessibleName() = default;

BEGIN_METADATA(SystemAppAccessibleName)
END_METADATA
