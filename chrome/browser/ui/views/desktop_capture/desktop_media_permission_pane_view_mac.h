// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PERMISSION_PANE_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PERMISSION_PANE_VIEW_MAC_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/view.h"

class DesktopMediaPermissionPaneViewMac : public views::View {
  METADATA_HEADER(DesktopMediaPermissionPaneViewMac, views::View)
 public:
  explicit DesktopMediaPermissionPaneViewMac(
      DesktopMediaList::Type type,
      base::RepeatingCallback<void()> open_screen_recording_settings_callback =
          base::RepeatingClosure());

  DesktopMediaPermissionPaneViewMac(const DesktopMediaPermissionPaneViewMac&) =
      delete;
  DesktopMediaPermissionPaneViewMac& operator=(
      const DesktopMediaPermissionPaneViewMac&) = delete;
  ~DesktopMediaPermissionPaneViewMac() override;

  bool WasPermissionButtonClicked() const;

  void SimulateClickForTesting();

 private:
  void OpenScreenRecordingSettingsPane();

  const DesktopMediaList::Type type_;
  base::RepeatingCallback<void()> open_screen_recording_settings_callback_;
  bool clicked_ = false;

  // `button_` is owned by `this` as a child view.
  raw_ptr<views::MdTextButton> button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PERMISSION_PANE_VIEW_MAC_H_
