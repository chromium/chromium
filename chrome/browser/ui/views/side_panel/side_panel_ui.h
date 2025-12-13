// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_H_

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}  // namespace content

// An abstract class of the side panel API. Get an instance of this class by
// calling BrowserWindowInterface->GetFeatures().side_panel_ui()
class SidePanelUI {
 public:
  // Open side panel with entry_id.
  virtual void Show(SidePanelEntryId entry_id,
                    std::optional<SidePanelOpenTrigger> open_trigger,
                    bool suppress_animations) = 0;
  void Show(SidePanelEntryId entry_id,
            std::optional<SidePanelOpenTrigger> open_trigger) {
    Show(entry_id, open_trigger, /*suppress_animations=*/false);
  }
  void Show(SidePanelEntryId entry_id) {
    Show(entry_id, std::nullopt, /*suppress_animations=*/false);
  }

  // Open side panel with entry key.
  virtual void Show(SidePanelEntryKey entry_key,
                    std::optional<SidePanelOpenTrigger> open_trigger,
                    bool suppress_animations) = 0;
  void Show(SidePanelEntryKey entry_key,
            std::optional<SidePanelOpenTrigger> open_trigger) {
    Show(entry_key, open_trigger, /*suppress_animations=*/false);
  }
  void Show(SidePanelEntryKey entry_key) {
    Show(entry_key, std::nullopt, /*suppress_animations=*/false);
  }

  // Open side panel with entry key, animating from
  // starting_bounds_in_browser_coordinates to its final open position.
  virtual void ShowFrom(SidePanelEntryKey entry_key,
                        gfx::Rect starting_bounds_in_browser_coordinates) = 0;

  // Close the side panel.
  virtual void Close(SidePanelEntry::PanelType panel_type,
                     SidePanelEntryHideReason hide_reason,
                     bool suppress_animations) = 0;
  void Close(SidePanelEntry::PanelType panel_type) {
    Close(panel_type, SidePanelEntryHideReason::kSidePanelClosed, false);
  }

  // Open the side panel for a key. If side panel for the key is already opened
  // then close the side panel.
  virtual void Toggle(SidePanelEntryKey key,
                      SidePanelOpenTrigger open_trigger) = 0;

  // Get the current entry id if the side panel is open.
  virtual std::optional<SidePanelEntryId> GetCurrentEntryId(
      SidePanelEntry::PanelType panel_type) const = 0;

  // Returns the current entries default width. Returns nullopt if this value is
  // not set or if the side panel is closed.
  virtual int GetCurrentEntryDefaultContentWidth(
      SidePanelEntry::PanelType type) const = 0;

  // Return whether any entry is being shown in the side panel.
  // Note: this returns false if `entry` is current loading but not actually
  // shown.
  virtual bool IsSidePanelShowing(SidePanelEntry::PanelType type) const = 0;

  // Returns whether `entry_key` is currently being shown in the side panel.
  // Note: this returns false if `entry` is current loading but not actually
  // shown.
  virtual bool IsSidePanelEntryShowing(
      const SidePanelEntryKey& entry_key) const = 0;

  // Similar to IsSidePanelEntryShowing, but restricts to either the tab-scoped
  // or window-scoped registry.
  virtual bool IsSidePanelEntryShowing(const SidePanelEntry::Key& entry_key,
                                       bool for_tab) const = 0;

  // Register for this callback to detect when the side panel opens or changes.
  // If the open is animated, this will be called at the beginning of the
  // animation.
  using ShownCallback = base::RepeatingCallback<void()>;
  virtual base::CallbackListSubscription RegisterSidePanelShown(
      SidePanelEntry::PanelType type,
      ShownCallback callback) = 0;

  // Returns the content view for the given entry. Returns nullptr if the entry
  // does not exist.
  virtual content::WebContents* GetWebContentsForTest(SidePanelEntryId id) = 0;

  virtual void DisableAnimationsForTesting() = 0;

  // Prevent content swapping delays from happening for testing.
  // This should be called before the side panel is first shown.
  virtual void SetNoDelaysForTesting(bool no_delays_for_testing) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_H_
