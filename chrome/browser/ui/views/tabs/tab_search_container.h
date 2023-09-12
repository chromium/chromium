// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class TabOrganizationButton;
class TabSearchButton;
class TabStrip;

class TabSearchContainer : public views::View {
 public:
  METADATA_HEADER(TabSearchContainer);
  TabSearchContainer(TabStrip* tab_strip, bool before_tab_strip);
  TabSearchContainer(const TabSearchContainer&) = delete;
  TabSearchContainer& operator=(const TabSearchContainer&) = delete;
  ~TabSearchContainer() override;

  TabOrganizationButton* tab_organization_button() {
    return tab_organization_button_;
  }
  TabSearchButton* tab_search_button() { return tab_search_button_; }

 private:
  raw_ptr<TabOrganizationButton, DanglingUntriaged> tab_organization_button_ =
      nullptr;
  raw_ptr<TabSearchButton, DanglingUntriaged> tab_search_button_ = nullptr;
  bool before_tab_strip_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_
