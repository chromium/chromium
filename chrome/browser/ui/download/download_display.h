// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_DISPLAY_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_DISPLAY_H_

#include <optional>
#include <string>

namespace offline_items_collection {
struct ContentId;
}

// This interface defines the Download Toolbar Button (a.k.a. Download Bubble).
class DownloadDisplay {
 public:
  // Determines how to draw the icon, based on the state of the underlying
  // downloads.
  enum class IconState {
    kProgress,
    kComplete,
    kDeepScanning,
  };

  // Whether the icon should be displayed in the active color (usually blue).
  enum class IconActive {
    kInactive,
    kActive,
  };

  // Determines how the progress ring and badge should be displayed in the icon.
  struct ProgressInfo {
    // Number of currently active downloads.
    int download_count = 0;
    // Percentage complete of all in-progress downloads.
    int progress_percentage = 0;
    // Whether we know the final size of all downloads.
    bool progress_certain = true;

    bool operator==(const ProgressInfo& other) const;
    bool operator!=(const ProgressInfo& other) const;

    // Compares all fields except the percentage.
    bool FieldsEqualExceptPercentage(const ProgressInfo& other) const;
  };

  // Describes updates to be made to the icon.
  struct IconUpdateInfo {
    // Nullopt indicates no change.
    std::optional<IconState> new_state = std::nullopt;
    std::optional<IconActive> new_active = std::nullopt;
    std::optional<ProgressInfo> new_progress = std::nullopt;

    // Whether an animated icon will be shown.
    bool show_animation = false;
  };

  // Shows the download display.
  virtual void Show() = 0;

  // Hides the download display immediately.
  virtual void Hide() = 0;

  // Returns whether or not the download display is visible.
  virtual bool IsShowing() const = 0;

  // Enables potential actions resulting from clicking the download display.
  virtual void Enable() = 0;

  // Disables potential actions resulting from clicking the download display.
  virtual void Disable() = 0;

  // Updates the download icon according to `new_state` and `new_active` and
  // potentially shows an animation. Updates the progress ring of the download
  // icon according to `new_progress` if provided.
  virtual void UpdateDownloadIcon(const IconUpdateInfo& updates) = 0;

  // Shows detailed information on the download display. It can be a popup or
  // dialog or partial view, essentially anything other than the main view.
  virtual void ShowDetails() = 0;

  // Hide the detailed information on the download display.
  virtual void HideDetails() = 0;

  // Returns whether the details are visible.
  virtual bool IsShowingDetails() const = 0;

  // Announces an accessible alert immediately.
  virtual void AnnounceAccessibleAlertNow(const std::u16string& alert_text) = 0;

  // Returns whether it is currently in fullscreen and the view that hosts the
  // download display is hidden.
  virtual bool IsFullscreenWithParentViewHidden() const = 0;

  // Whether we should show the exclusive access bubble upon starting a download
  // in fullscreen mode. If the user cannot exit fullscreen, there is no point
  // in showing an exclusive access bubble telling the user to exit fullscreen
  // to view their downloads, because exiting is impossible. If we are in
  // immersive fullscreen mode, we don't need to show the exclusive access
  // bubble because we will just temporarily reveal the toolbar when the
  // downloads finish.
  virtual bool ShouldShowExclusiveAccessBubble() const = 0;

  // Open the security subpage for the download with `id`, if it exists.
  virtual void OpenSecuritySubpage(
      const offline_items_collection::ContentId& id) = 0;

  // Opens the primary dialog to the item and scrolls to the item, and opens
  // the security dialog if the item has a security warning. Returns whether
  // bubble was opened to the requested item.
  virtual bool OpenMostSpecificDialog(
      const offline_items_collection::ContentId& content_id) = 0;

  // Gets the current icon state.
  virtual IconState GetIconState() const = 0;

 protected:
  virtual ~DownloadDisplay() = default;
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_DISPLAY_H_
