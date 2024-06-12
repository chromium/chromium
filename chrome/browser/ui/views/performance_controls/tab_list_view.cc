// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/tab_list_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/performance_controls/tab_list_model.h"
#include "chrome/browser/ui/views/performance_controls/tab_list_row_view.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

TabListView::TabListView(TabListModel* tab_list_model)
    : tab_list_model_(tab_list_model) {
  views::FlexLayout* const flex_layout =
      views::View::SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical);

  for (resource_attribution::PageContext context :
       tab_list_model->page_contexts()) {
    AddChildView(std::make_unique<TabListRowView>(
        context, tab_list_model,
        base::BindOnce(&TabListView::RemoveRow, base::Unretained(this),
                       context)));
  }
}

TabListView::~TabListView() = default;

void TabListView::RemoveRow(resource_attribution::PageContext context,
                            TabListRowView* row_view) {
  tab_list_model_->RemovePageContext(context);
  RemoveChildViewT(row_view);
}

BEGIN_METADATA(TabListView)
END_METADATA
