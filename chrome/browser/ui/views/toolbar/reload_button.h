// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "chrome/browser/ui/views/toolbar/reload_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/viz/common/frame_timing_details.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/metadata/view_factory.h"

class CommandUpdater;
class Profile;
class WaapUIMetricsRecorder;
class WaapUIMetricsRecorder;

////////////////////////////////////////////////////////////////////////////////
//
// ReloadButton
//
// The reload button in the toolbar, which changes to a stop button when a page
// load is in progress. The change from stop back to reload may be delayed if
// the user is hovering the button, to prevent mis-clicks.
//
////////////////////////////////////////////////////////////////////////////////

class ReloadButton : public ToolbarButton, public ReloadControl {
  METADATA_HEADER(ReloadButton, ToolbarButton)

 public:
  ReloadButton(Profile* profile, CommandUpdater* command_updater);
  ReloadButton(const ReloadButton&) = delete;
  ReloadButton& operator=(const ReloadButton&) = delete;
  ~ReloadButton() override;

  Mode visible_mode() const { return visible_mode_; }

  void SetVectorIconsForMode(Mode mode,
                             const gfx::VectorIcon& icon,
                             const gfx::VectorIcon& touch_icon);

  // ToolbarButton:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool ShouldShowMenu() override;
  void ShowDropDownMenu(ui::mojom::MenuSourceType source_type) override;

  void UpdateCachedTooltipText();

  // Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // ReloadControl overrides:
  void ChangeMode(Mode mode, bool force) override;
  bool GetMenuEnabled() const override;
  void SetMenuEnabled(bool is_menu_enabled) override;
  views::View* GetAsViewClassForTesting() override;

  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  friend class ReloadButtonMetricsTest;
  friend class ReloadButtonTestBase;
  FRIEND_TEST_ALL_PREFIXES(ReloadButtonTest, TooltipText);
  FRIEND_TEST_ALL_PREFIXES(ReloadButtonTest, TooltipTextAccessibility);

  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

  void SetVisibleMode(Mode mode);

  void ButtonPressed(const ui::Event& event);

  void ExecuteBrowserCommand(int command, int event_flags);

  void OnDoubleClickTimer();
  void OnStopToReloadTimer();
  void UpdateAccessibleHasPopup();
  void OnNextPresentation(Mode mode,
                          Button::ButtonState state,
                          const viz::FrameTimingDetails&);

  base::OneShotTimer double_click_timer_;

  // Timer to delay switching between reload and stop states.
  base::OneShotTimer mode_switch_timer_;

  // This can't be null. But it may not record anything if the feature is
  // disabled or if profile is missing.
  const std::unique_ptr<WaapUIMetricsRecorder> metrics_recorder_;

  // This may be NULL when testing.
  raw_ptr<CommandUpdater, DanglingUntriaged> command_updater_;

  // Vector icons to use for both modes.
  base::raw_ref<const gfx::VectorIcon> reload_icon_;
  base::raw_ref<const gfx::VectorIcon> reload_touch_icon_;
  base::raw_ref<const gfx::VectorIcon> stop_icon_;
  base::raw_ref<const gfx::VectorIcon> stop_touch_icon_;

  // The mode we should be in assuming no timers are running.
  Mode intended_mode_ = Mode::kReload;

  // The currently-visible mode - this may differ from the intended mode.
  Mode visible_mode_ = Mode::kReload;

  // The delay times for the timers.  These are members so that tests can modify
  // them.
  base::TimeDelta double_click_timer_delay_;
  base::TimeDelta mode_switch_timer_delay_;

  // Indicates if reload menu is enabled.
  bool is_menu_enabled_ = false;

  // TESTING ONLY
  // True if we should pretend the button is hovered.
  bool testing_mouse_hovered_ = false;
  // Increments when we would tell the browser to "reload", so
  // test code can tell whether we did so (as there may be no |browser_|).
  int testing_reload_count_ = 0;

  // Must be the last member.
  base::WeakPtrFactory<ReloadButton> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(CHROME_VIEWS_EXPORT, ReloadButton, ToolbarButton)
VIEW_BUILDER_PROPERTY(bool, MenuEnabled)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(CHROME_VIEWS_EXPORT, ReloadButton)

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_H_
