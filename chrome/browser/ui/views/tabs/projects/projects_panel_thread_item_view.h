// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_THREAD_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_THREAD_ITEM_VIEW_H_

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace contextual_tasks {
struct Thread;
}

namespace views {
class Label;
}

// Displays a button for a single thread (e.g., a Gemini or AIM thread).
class ProjectsPanelThreadItemView : public views::Button {
  METADATA_HEADER(ProjectsPanelThreadItemView, views::Button)

 public:
  using ThreadPressedCallback =
      base::RepeatingCallback<void(const std::string&,
                                   contextual_tasks::ThreadType)>;

  explicit ProjectsPanelThreadItemView(
      const contextual_tasks::Thread& task,
      ThreadPressedCallback pressed_callback = base::DoNothing());
  ProjectsPanelThreadItemView(const ProjectsPanelThreadItemView&) = delete;
  ProjectsPanelThreadItemView& operator=(const ProjectsPanelThreadItemView&) =
      delete;
  ~ProjectsPanelThreadItemView() override;

  // views::Button:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  const views::Label* title_for_testing() { return title_; }

  const gfx::VectorIcon& chat_type_icon_for_testing() {
    return *chat_type_icon_;
  }

  const views::ImageView* trailing_icon_for_testing() { return trailing_icon_; }

  static void disable_animations_for_testing();

 private:
  void UpdateHoverState();
  void UpdateHoverStateForced(bool is_hovered);

  raw_ptr<views::Label> title_ = nullptr;
  raw_ref<const gfx::VectorIcon> chat_type_icon_;
  raw_ptr<views::ImageView> trailing_icon_ = nullptr;

  gfx::SlideAnimation trailing_icon_fade_animation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_THREAD_ITEM_VIEW_H_
