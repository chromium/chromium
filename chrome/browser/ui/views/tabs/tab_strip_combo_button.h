// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_COMBO_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_COMBO_BUTTON_H_

#include "base/memory/raw_ref.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

class BrowserWindowInterface;
class TabSearchContainer;
class TabStrip;

class TabStripComboButton : public views::View {
  METADATA_HEADER(TabStripComboButton, views::View)

 public:
  TabStripComboButton(BrowserWindowInterface* browser, TabStrip* tab_strip);

  TabStripComboButton(const TabStripComboButton&) = delete;
  TabStripComboButton& operator=(const TabStripComboButton&) = delete;
  ~TabStripComboButton() override;

  void OnNewTabButtonStateChanged();
  void OnTabSearchButtonStateChanged();
  void UpdateSeparatorVisibility();

  views::Button* new_tab_button() { return new_tab_button_; }

  TabSearchContainer* tab_search_container() { return tab_search_container_; }

  views::Separator* separator() { return separator_; }

 private:
  raw_ptr<views::Button> new_tab_button_ = nullptr;
  raw_ptr<TabSearchContainer> tab_search_container_ = nullptr;
  raw_ptr<views::Separator> separator_ = nullptr;

  base::TimeTicks new_tab_button_last_pressed_;
  base::TimeTicks tab_search_button_last_pressed_;
  std::list<base::CallbackListSubscription> subscriptions_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_COMBO_BUTTON_H_
