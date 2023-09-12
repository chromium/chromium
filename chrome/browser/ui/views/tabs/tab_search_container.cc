// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"

TabSearchContainer::TabSearchContainer(TabStrip* tab_strip) {
  std::unique_ptr<TabSearchButton> tab_search_button =
      std::make_unique<TabSearchButton>(tab_strip);
  tab_search_button->SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kCenter);

  tab_search_button_ = AddChildView(std::move(tab_search_button));

  if (features::IsTabOrganization()) {
    tab_organization_button_ =
        AddChildView(std::make_unique<TabOrganizationButton>(tab_strip));
    tab_organization_button_->SetProperty(views::kCrossAxisAlignmentKey,
                                          views::LayoutAlignment::kCenter);
  }

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

TabSearchContainer::~TabSearchContainer() = default;

BEGIN_METADATA(TabSearchContainer, views::View)
END_METADATA
