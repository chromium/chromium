// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_H_

#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"

namespace content {
class WebContents;
}  // namespace content

// An abstract class of the side panel API. Get an instance of this class by
// calling BrowserWindowInterface->GetFeatures().side_panel_ui()
class SidePanelUI {
 public:
  // Open side panel with entry_id.
  virtual void Show(
      SidePanelEntryId entry_id,
      std::optional<SidePanelOpenTrigger> open_trigger = std::nullopt) = 0;

  // Open side panel with entry key.
  virtual void Show(
      SidePanelEntryKey entry_key,
      std::optional<SidePanelOpenTrigger> open_trigger = std::nullopt) = 0;

  // Close the side panel.
  virtual void Close() = 0;

  // Open the side panel for a key. If side panel for the key is already opened
  // then close the side panel.
  virtual void Toggle(SidePanelEntryKey key,
                      SidePanelOpenTrigger open_trigger) = 0;

  // Opens the current side panel contents in a new tab. This is called by the
  // header button, when it's visible.
  virtual void OpenInNewTab() = 0;

  // Toggle the pin state. This is called by the header button, when it's
  // visible.
  virtual void UpdatePinState() = 0;

  // Get the current entry id if the side panel is open.
  virtual std::optional<SidePanelEntryId> GetCurrentEntryId() const = 0;

  // Return whether any entry is being shown in the side panel.
  // Note: this returns false if `entry` is current loading but not actually
  // shown.
  virtual bool IsSidePanelShowing() const = 0;

  // Returns whether `entry_key` is currently being shown in the side panel.
  // Note: this returns false if `entry` is current loading but not actually
  // shown.
  virtual bool IsSidePanelEntryShowing(
      const SidePanelEntryKey& entry_key) const = 0;

  // Returns the content view for the given entry. Returns nullptr if the entry
  // does not exist.
  virtual content::WebContents* GetWebContentsForTest(SidePanelEntryId id) = 0;

  virtual void DisableAnimationsForTesting() = 0;

  // Prevent content swapping delays from happening for testing.
  // This should be called before the side panel is first shown.
  virtual void SetNoDelaysForTesting(bool no_delays_for_testing) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_H_
