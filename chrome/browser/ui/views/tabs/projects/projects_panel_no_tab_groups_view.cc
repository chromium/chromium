// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_no_tab_groups_view.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

ProjectsPanelNoTabGroupsView::ProjectsPanelNoTabGroupsView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::BoxLayout::Orientation::kVertical);
  // TODO(crbug.com/480202345) Add in image asset when it's available.
  auto* no_groups_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_TAB_GROUPS_NO_TAB_GROUPS)));
  no_groups_label->SetEnabledColor(ui::kColorSysOnSurfaceSubtle);
  no_groups_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  no_groups_label->SetTextStyle(views::style::STYLE_BODY_4);
  no_groups_label->SetMultiLine(true);
}

ProjectsPanelNoTabGroupsView::~ProjectsPanelNoTabGroupsView() = default;

BEGIN_METADATA(ProjectsPanelNoTabGroupsView)
END_METADATA
