// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_UNFOCUS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_UNFOCUS_BUTTON_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "components/tab_groups/tab_group_color.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

class TabGroupUnfocusButton : public views::LabelButton {
  METADATA_HEADER(TabGroupUnfocusButton, views::LabelButton)

 public:
  TabGroupUnfocusButton(TabStripOrientation orientation,
                        base::RepeatingClosure callback);
  TabGroupUnfocusButton(const TabGroupUnfocusButton&) = delete;
  TabGroupUnfocusButton& operator=(const TabGroupUnfocusButton&) = delete;
  ~TabGroupUnfocusButton() override;

  void SetGroupColorData(tab_groups::TabGroupColorId color_id,
                         bool is_ephemeral);
  void UpdateColors();

  // views::LabelButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

 private:
  const TabStripOrientation orientation_;
  tab_groups::TabGroupColorId color_id_ = tab_groups::TabGroupColorId::kGrey;
  bool is_ephemeral_ = false;
  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_UNFOCUS_BUTTON_H_
