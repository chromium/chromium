// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_UNFOCUS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_UNFOCUS_BUTTON_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_focus_animation_state.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "components/tab_groups/tab_group_color.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

class TabGroup;

class TabGroupUnfocusButton : public views::LabelButton {
  METADATA_HEADER(TabGroupUnfocusButton, views::LabelButton)

 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void ExitFocusMode() = 0;
    virtual bool IsValid() const = 0;
    virtual const TabGroup& GetTabGroup() const = 0;
  };

  TabGroupUnfocusButton(Delegate& delegate, TabStripOrientation orientation);
  TabGroupUnfocusButton(const TabGroupUnfocusButton&) = delete;
  TabGroupUnfocusButton& operator=(const TabGroupUnfocusButton&) = delete;
  ~TabGroupUnfocusButton() override;

  void SetGroupColorData(tab_groups::TabGroupColorId color_id,
                         bool is_ephemeral);

  // `state.progress` drives the corners abutting the header, from 0.0 (fully
  // rounded) to 1.0 (flattened, as when focused).
  void SetFocusAnimationState(const TabGroupFocusAnimationState& state);
  void UpdateColors();
  void UpdateAccessibleName();

  // views::LabelButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

 private:
  SkColor GetBaseBackgroundColor() const;
  SkColor GetBackgroundColor() const;
  SkColor GetForegroundColor() const;
  // Foreground color with the focus state's content opacity applied.
  SkColor GetTextColor() const;
  void UpdateBorder();
  void UpdateCollapsedState();
  void UpdateTextAndIcon();

  const raw_ref<Delegate> delegate_;
  const TabStripOrientation orientation_;
  tab_groups::TabGroupColorId color_id_ = tab_groups::TabGroupColorId::kGrey;
  bool is_ephemeral_ = false;
  bool is_collapsed_ = false;
  TabGroupFocusAnimationState focus_animation_state_;
  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_UNFOCUS_BUTTON_H_
