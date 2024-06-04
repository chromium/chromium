// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PERMISSION_PANE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PERMISSION_PANE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class DesktopMediaPermissionPaneView : public views::View {
  METADATA_HEADER(DesktopMediaPermissionPaneView, views::View)
 public:
  explicit DesktopMediaPermissionPaneView(DesktopMediaList::Type type);

  DesktopMediaPermissionPaneView(const DesktopMediaPermissionPaneView&) =
      delete;
  DesktopMediaPermissionPaneView& operator=(
      const DesktopMediaPermissionPaneView&) = delete;
  ~DesktopMediaPermissionPaneView() override;

  bool WasPermissionButtonClicked() const;

 private:
  void OpenScreenRecordingSettingsPane();

  const DesktopMediaList::Type type_;
  bool clicked_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PERMISSION_PANE_VIEW_H_
