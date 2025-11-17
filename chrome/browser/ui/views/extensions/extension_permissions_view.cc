// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_permissions_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/expandable_container_view.h"
#include "extensions/browser/install_prompt_permissions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

ExtensionPermissionsView::ExtensionPermissionsView(
    const extensions::InstallPromptPermissions& permissions) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  for (size_t i = 0; i < permissions.permissions.size(); ++i) {
    AddItem(permissions.permissions.at(i), permissions.details.at(i));
  }
}

void ExtensionPermissionsView::AddItem(
    const std::u16string& permission_text,
    const std::u16string& permission_details) {
  auto permission_label =
      views::Builder<views::Label>()
          .SetText(permission_text)
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetMultiLine(true)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build();
  AddChildView(std::move(permission_label));

  if (!permission_details.empty()) {
    // If we have more details to provide, show them in collapsed form.
    AddChildView(std::make_unique<ExpandableContainerView>(permission_details));
  }
}

BEGIN_METADATA(ExtensionPermissionsView)
END_METADATA
