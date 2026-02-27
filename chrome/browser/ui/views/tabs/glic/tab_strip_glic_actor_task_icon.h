// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_ACTOR_TASK_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_ACTOR_TASK_ICON_H_

#include <string>

#include "base/callback_list.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"

class BrowserWindowInterface;

namespace glic {

class TabStripGlicActorTaskIcon : public TabStripNudgeButton {
  METADATA_HEADER(TabStripGlicActorTaskIcon, TabStripNudgeButton)

 public:
  explicit TabStripGlicActorTaskIcon(
      BrowserWindowInterface* browser_window_interface,
      PressedCallback pressed_callback);
  TabStripGlicActorTaskIcon(const TabStripGlicActorTaskIcon&) = delete;
  TabStripGlicActorTaskIcon& operator=(const TabStripGlicActorTaskIcon&) =
      delete;
  ~TabStripGlicActorTaskIcon() override;

  // TabStripControlButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // TabStripNudgeButton:
  void SetIsShowingNudge(bool is_showing) override;

  // Sets the task icon back to its default colors.
  void SetDefaultColors();

  // Sets the task icon's color to its pressed state color if `is_pressed` is
  // true, or to its default color otherwise.
  void SetPressedColor(bool is_pressed);

  // Show the task nudge with the given text.
  void ShowNudgeLabel(const std::u16string nudge_label);

  // Sets the task icon to its default colors, label, and tooltip text.
  void SetTaskIconToDefault();

  // Updates the background painter to match the current border insets.
  void RefreshBackground();

  // Defines how the button calculates its width during animation.
  enum class AnimationMode {
    kEntry,  // Animating from 0 width -> icon width
    kNudge   // Animating from icon width -> full nudge width
  };

  void SetAnimationMode(AnimationMode mode);
  AnimationMode GetAnimationMode() const { return animation_mode_; }

  // GetBoundsInScreen() gives a rect with some padding that extends beyond the
  // visible edges of the button. This function returns a rect without that
  // padding in order to anchor the ActorTaskListBubble on the edge of the
  // button.
  gfx::Rect GetAnchorBoundsInScreen() const override;

 private:
  void OnBrowserWindowDidBecomeActive(BrowserWindowInterface* bwi);
  void OnBrowserWindowDidBecomeInactive(BrowserWindowInterface* bwi);
  void UpdateInkdropHoverColor(bool is_frame_active);

  void NotifyClick(const ui::Event& event) override;

  // Whether or not to use the same background as the alt icon, which may be
  // enabled as part of GlicEntrypointVariations. Should be kept in sync with
  // TapStripGlicButton::ShouldUseAltIcon.
  bool ShouldUseGlicButtonAltIconBackgroundColor();

  AnimationMode animation_mode_ = AnimationMode::kEntry;
  base::CallbackListSubscription window_did_become_active_subscription_;
  base::CallbackListSubscription window_did_become_inactive_subscription_;

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_ACTOR_TASK_ICON_H_
