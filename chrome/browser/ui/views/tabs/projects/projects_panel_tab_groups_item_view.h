// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_ITEM_VIEW_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/uuid.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace views {
class Label;
class MenuButton;
}  // namespace views

// Contains the Tab Groups inside the Projects Panel.
class ProjectsPanelTabGroupsItemView : public views::Button {
  METADATA_HEADER(ProjectsPanelTabGroupsItemView, views::Button)

 public:
  using TabGroupPressedCallback =
      base::RepeatingCallback<void(const base::Uuid&)>;
  using MoreButtonPressedCallback =
      base::RepeatingCallback<void(const base::Uuid&, views::MenuButton&)>;

  explicit ProjectsPanelTabGroupsItemView(
      const tab_groups::SavedTabGroup& group,
      TabGroupPressedCallback pressed_callback = base::DoNothing(),
      MoreButtonPressedCallback more_button_callback = base::DoNothing());
  ProjectsPanelTabGroupsItemView(const ProjectsPanelTabGroupsItemView&) =
      delete;
  ProjectsPanelTabGroupsItemView& operator=(
      const ProjectsPanelTabGroupsItemView&) = delete;
  ~ProjectsPanelTabGroupsItemView() override;

  const base::Uuid& guid() const { return group_guid_; }

  void SetIsDragging(bool dragging);
  bool is_dragging() const { return dragging_; }

  // Returns the image used during dragging.
  gfx::ImageSkia GetDragImage();

  // views::View:
  void PaintChildren(const views::PaintInfo& paint_info) override;
  void OnThemeChanged() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnDragDone() override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  views::Label* title_for_testing() { return title_; }
  views::MenuButton* more_button_for_testing() { return more_button_; }
  views::ImageView* shared_icon_for_testing() { return shared_icon_; }

  const gfx::VectorIcon& tab_group_vector_icon_for_testing() {
    return *tab_group_vector_icon_;
  }

  static void disable_animations_for_testing();

 private:
  void OnMoreButtonPressed();
  void OnMoreButtonStateChanged();

  void UpdateHoverState();

  const base::Uuid group_guid_;
  MoreButtonPressedCallback more_button_callback_;

  // Whether this item is currently being dragged.
  bool dragging_ = false;

  gfx::SlideAnimation button_fade_animation_{this};

  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::ImageView> tab_group_icon_ = nullptr;
  const tab_groups::TabGroupColorId tab_group_color_id_;
  raw_ref<const gfx::VectorIcon> tab_group_vector_icon_;
  raw_ptr<views::ImageView> shared_icon_ = nullptr;
  raw_ptr<views::MenuButton> more_button_ = nullptr;
  base::CallbackListSubscription more_button_state_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_ITEM_VIEW_H_
