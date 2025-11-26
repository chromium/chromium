// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_SIDE_PANEL_UI_H_

#include <optional>

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui_base.h"

class Browser;
class WebUIBrowserWindow;

namespace views {
class View;
}  // namespace views

class WebUIBrowserSidePanelUI : public SidePanelUIBase {
 public:
  explicit WebUIBrowserSidePanelUI(Browser* browser);
  ~WebUIBrowserSidePanelUI() override;

  // SidePanelUI:
  void Close(SidePanelEntry::PanelType panel_type,
             SidePanelEntryHideReason reason,
             bool suppress_animations) override;
  void Toggle(SidePanelEntryKey key,
              SidePanelOpenTrigger open_trigger) override;
  void ShowFrom(SidePanelEntryKey entry_key,
                gfx::Rect starting_bounds_in_browser_coordinates) override;
  content::WebContents* GetWebContentsForTest(SidePanelEntryId id) override;
  void DisableAnimationsForTesting() override;
  void SetNoDelaysForTesting(bool no_delays_for_testing) override;

  content::WebContents* GetWebContentsForId(SidePanelEntryId entry_id) const;

  void OnSidePanelClosed(SidePanelEntry::PanelType type);

 private:
  // SidePanelUIBase:
  void Show(const UniqueKey& entry,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
            bool suppress_animations) override;
  void PopulateSidePanel(
      bool suppress_animations,
      const UniqueKey& unique_key,
      std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
      SidePanelEntry* entry,
      std::optional<std::unique_ptr<views::View>> content_view) override;
  void MaybeShowEntryOnTabStripModelChanged(
      SidePanelRegistry* old_contextual_registry,
      SidePanelRegistry* new_contextual_registry) override;

  WebUIBrowserWindow* GetWebUIBrowserWindow();

  // Owns the WebView of this side panel. The WebView itself is never rendered.
  // Instead, WebViews containing WebContents will be hosted by Webium in a
  // guest contents.
  std::unique_ptr<views::View> current_side_panel_view_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_SIDE_PANEL_UI_H_
