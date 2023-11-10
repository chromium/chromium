// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UI_H_

#include "base/supports_user_data.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"

class Browser;

namespace content {
class WebContents;
}  // namespace content

// An abstract class of the side panel API.
// The class is created in BrowserView for desktop Chrome. Get the instance of
// this class by calling SidePanelUI::GetSidePanelUIForBrowser(browser);
class SidePanelUI : public base::SupportsUserData::Data {
 public:
  SidePanelUI() = default;
  SidePanelUI(const SidePanelUI&) = delete;
  SidePanelUI& operator=(const SidePanelUI&) = delete;
  ~SidePanelUI() override = default;

  static SidePanelUI* GetSidePanelUIForBrowser(Browser* browser);

  static void SetSidePanelUIForBrowser(
      Browser* browser,
      std::unique_ptr<SidePanelUI> side_panel_ui);

  static void RemoveSidePanelUIForBrowser(Browser* browser);

  // Open side panel with entry_id.
  virtual void Show(
      absl::optional<SidePanelEntryId> entry_id = absl::nullopt,
      absl::optional<SidePanelOpenTrigger> open_trigger = absl::nullopt) = 0;

  // Open side panel with entry key.
  virtual void Show(
      SidePanelEntryKey entry_key,
      absl::optional<SidePanelOpenTrigger> open_trigger = absl::nullopt) = 0;

  // Close the side panel.
  virtual void Close() = 0;

  // Open side panel when it's close or close side panel when it's open.
  // TODO(shibalik): Remove after SidePanelPinning launch.
  virtual void Toggle() = 0;

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
  virtual absl::optional<SidePanelEntryId> GetCurrentEntryId() const = 0;

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

 private:
  static const int kUserDataKey = 0;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UI_H_
