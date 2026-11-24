// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_permissions_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/expandable_container_view.h"
#include "extensions/browser/install_prompt_permissions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

ExtensionPermissionsView::ExtensionPermissionsView(
    const extensions::InstallPromptPermissions& permissions,
    bool show_bullet_points)
    : show_bullet_points_(show_bullet_points) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  if (show_bullet_points_) {
    // Ignore this wrapper view so its `kListItem` children are direct children
    // of the enclosing `kList` permissions container.
    GetViewAccessibility().SetIsIgnored(true);
  }

  for (size_t i = 0; i < permissions.permissions.size(); ++i) {
    AddItem(permissions.permissions.at(i), permissions.visible_details.at(i),
            permissions.collapsed_details.at(i));
  }
}

void ExtensionPermissionsView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ExtensionPermissionsView::AddItem(
    const std::u16string& permission_text,
    const std::u16string& visible_details,
    const std::u16string& collapsed_details) {
  views::View* item_parent = this;
  int details_indent = 0;

  if (show_bullet_points_) {
    auto item_container = std::make_unique<views::BoxLayoutView>();
    item_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
    item_container->SetBetweenChildSpacing(
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL));
    item_container->GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
    item_container->GetViewAccessibility().SetName(permission_text);

    auto permission_row = std::make_unique<views::BoxLayoutView>();
    permission_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    permission_row->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    permission_row->GetViewAccessibility().SetIsIgnored(true);

    auto* bullet = permission_row->AddChildView(
        views::Builder<views::Label>()
            .SetText(u"• ")
            .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
            .SetTextStyle(views::style::STYLE_PRIMARY)
            .Build());
    bullet->GetViewAccessibility().SetIsIgnored(true);

    auto* permission_label = permission_row->AddChildView(
        views::Builder<views::Label>()
            .SetText(permission_text)
            .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
            .SetTextStyle(views::style::STYLE_PRIMARY)
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .Build());
    permission_label->GetViewAccessibility().SetIsIgnored(true);
    permission_row->SetFlexForView(permission_label, 1);

    details_indent = bullet->GetPreferredSize().width();
    item_container->AddChildView(std::move(permission_row));
    item_parent = AddChildView(std::move(item_container));
  } else {
    item_parent->AddChildView(
        views::Builder<views::Label>()
            .SetText(permission_text)
            .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
            .SetTextStyle(views::style::STYLE_SECONDARY)
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .Build());
  }

  if (!collapsed_details.empty()) {
    // If we have more details to provide, show them in expandable form.
    auto expandable = std::make_unique<ExpandableContainerView>(
        visible_details, collapsed_details);
    if (details_indent > 0) {
      expandable->SetBorder(
          views::CreateEmptyBorder(gfx::Insets::TLBR(0, details_indent, 0, 0)));
    }
    item_parent->AddChildView(std::move(expandable));
  } else if (!visible_details.empty()) {
    auto visible_details_label =
        views::Builder<views::Label>()
            .SetText(visible_details)
            .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
            .SetTextStyle(views::style::STYLE_SECONDARY)
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .Build();
    if (details_indent > 0) {
      visible_details_label->SetBorder(
          views::CreateEmptyBorder(gfx::Insets::TLBR(0, details_indent, 0, 0)));
    }
    item_parent->AddChildView(std::move(visible_details_label));
  }
}

BEGIN_METADATA(ExtensionPermissionsView)
END_METADATA
