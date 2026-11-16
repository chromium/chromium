// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_ACTOR_TASK_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_ACTOR_TASK_ICON_H_

#include "chrome/browser/ui/views/glic/glic_actor_task_icon.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_actor_task_icon_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_constants.h"

class BrowserWindowInterface;

namespace glic {
class ToolbarGlicActorTaskIcon : public GlicActorTaskIcon<ToolbarButton>,
                                 public ToolbarGlicActorTaskIconInterface {
  METADATA_HEADER(ToolbarGlicActorTaskIcon, ToolbarButton)
 public:
  explicit ToolbarGlicActorTaskIcon(
      BrowserWindowInterface* browser_window_interface,
      PressedCallback pressed_callback);
  ToolbarGlicActorTaskIcon(const ToolbarGlicActorTaskIcon&) = delete;
  ToolbarGlicActorTaskIcon& operator=(const ToolbarGlicActorTaskIcon&) = delete;
  ~ToolbarGlicActorTaskIcon() override;

  // ToolbarGlicActorTaskIconInterface:
  void SetVisible(bool visible) override;
  bool IsVisible() const override;
  void SetIsShowingNudge(bool is_showing) override;
  bool GetIsShowingNudge() const override;
  void ShowNudgeLabel(const std::u16string& nudge_label) override;
  void SetTaskIconToDefault() override;
  void SetPressedState(bool is_pressed) override;
  void GetBubbleAnchorAsync(
      base::OnceCallback<void(views::BubbleAnchor)> callback) override;

  void AddedToWidget() override;
  gfx::Size GetMinimumSize() const override;
  void SetForegroundFrameActiveColorId(ui::ColorId new_color_id) override;
  void SetForegroundFrameInactiveColorId(ui::ColorId new_color_id) override;
  void SetBackgroundFrameActiveColorId(ui::ColorId new_color_id) override;
  void SetBackgroundFrameInactiveColorId(ui::ColorId new_color_id) override;
  void UpdateColors() override;
  void UpdateColorsAndInsets() override;

  void SetLeftRightCornerRadii(int left, int right) override;
  float GetCornerRadiusFor(ToolbarButton::Edge edge) const override;
  int GetSplitRoundedEdgeRadius() override;

 private:
  int split_rounded_edge_radius_ = kDefaultSplitButtonRoundedCornerRadius;

  std::optional<int> left_corner_radius_;
  std::optional<int> right_corner_radius_;
};
}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_ACTOR_TASK_ICON_H_
