// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_permissions_view.h"

#include "chrome/browser/extensions/install_prompt_permissions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/expandable_container_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

ExtensionPermissionsView::ExtensionPermissionsView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
}

void ExtensionPermissionsView::AddItem(
    const std::u16string& permission_text,
    const std::u16string& permission_details) {
  auto permission_label = std::make_unique<views::Label>(
      permission_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  permission_label->SetMultiLine(true);
  permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(permission_label));
  if (!permission_details.empty()) {
    // If we have more details to provide, show them in collapsed form.
    std::vector<std::u16string> details_container;
    details_container.push_back(permission_details);
    AddChildView(std::make_unique<ExpandableContainerView>(details_container));
  }
}

void ExtensionPermissionsView::AddPermissions(
    const extensions::InstallPromptPermissions& permissions) {
  for (size_t i = 0; i < permissions.permissions.size(); ++i) {
    AddItem(permissions.permissions[i], permissions.details[i]);
  }
}

void ExtensionPermissionsView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

BEGIN_METADATA(ExtensionPermissionsView)
END_METADATA
