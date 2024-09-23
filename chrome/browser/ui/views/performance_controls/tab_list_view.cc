// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/tab_list_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/performance_controls/tab_list_model.h"
#include "chrome/browser/ui/views/performance_controls/tab_list_row_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

TabListView::TabListView(TabListModel* tab_list_model)
    : tab_list_model_(tab_list_model) {
  views::FlexLayout* const flex_layout =
      views::View::SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical);

  views::FocusManager::set_arrow_key_traversal_enabled(true);

  for (resource_attribution::PageContext context :
       tab_list_model->page_contexts()) {
    AddChildView(std::make_unique<TabListRowView>(
        context, tab_list_model,
        base::BindOnce(&TabListView::RemoveRow, base::Unretained(this),
                       context)));
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
}

TabListView::~TabListView() = default;

void TabListView::RemoveRow(resource_attribution::PageContext context,
                            TabListRowView* row_view) {
  tab_list_model_->RemovePageContext(context);
  // Explicitly remove the ink drop for the row view to so that the InkDrop
  // doesn't access the non-override versions of Add/RemoveLayerFromRegions()
  // while the child is being removed from this view.
  views::InkDrop::Remove(row_view);
  RemoveChildViewT(row_view);
  RecordTabRemovedFromTabList(tab_list_model_->count());
}

void TabListView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetNameChecked(l10n_util::GetPluralStringFUTF16(
      IDS_PERFORMANCE_INTERVENTION_TAB_LIST_ACCNAME, tab_list_model_->count()));
}

BEGIN_METADATA(TabListView)
END_METADATA
