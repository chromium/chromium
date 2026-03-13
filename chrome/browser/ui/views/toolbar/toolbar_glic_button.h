// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_BUTTON_H_

#include "chrome/browser/ui/views/glic/glic_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserFrameView;

namespace glic {
class ToolbarGlicButton : public GlicButton<ToolbarButton> {
  METADATA_HEADER(ToolbarGlicButton, ToolbarButton)
 public:
  explicit ToolbarGlicButton(
      BrowserWindowInterface* browser_window_interface,
      base::RepeatingClosure hovered_callback,
      base::RepeatingClosure mouse_down_callback,
      base::RepeatingClosure expansion_animation_done_callback,
      const std::u16string& tooltip,
      PressedCallback pressed_callback);
  ToolbarGlicButton(const ToolbarGlicButton&) = delete;
  ToolbarGlicButton& operator=(const ToolbarGlicButton&) = delete;
  ~ToolbarGlicButton() override;

  void SetCloseButtonFocusBehavior(
      views::View::FocusBehavior focus_behavior) override;

  bool IsWidgetAlive() const;
  void AddedToWidget() override;
  void UpdateColors() override;
  void SetWidthFactor(float factor);
  void AddCloseButton(PressedCallback pressed_callback);
  BrowserFrameView* GetBrowserFrameView() const;
  void SetForegroundFrameActiveColorId(ui::ColorId new_color_id) override;
  void SetForegroundFrameInactiveColorId(ui::ColorId new_color_id) override;
  void SetBackgroundFrameActiveColorId(ui::ColorId new_color_id) override;
  void SetBackgroundFrameInactiveColorId(ui::ColorId new_color_id) override;
  ui::ColorId GetBackgroundColor();

 private:
  void UpdateBackground();
  void UpdateInkDrop();
};
}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_BUTTON_H_
