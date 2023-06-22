// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PANE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PANE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/desktop_capture/share_audio_view.h"

class DesktopMediaPaneView : public views::View {
 public:
  // Creates a pane-view with the supplied content_view. If share_audio_view !=
  // nullptr, it is added below content_view.
  DesktopMediaPaneView(std::unique_ptr<views::View> content_view,
                       std::unique_ptr<ShareAudioView> share_audio_view);

  DesktopMediaPaneView(const DesktopMediaPaneView&) = delete;
  DesktopMediaPaneView& operator=(const DesktopMediaPaneView&) = delete;
  ~DesktopMediaPaneView() override;

  bool AudioOffered() const;
  bool IsAudioSharingApprovedByUser() const;
  // Sets the state of the ShareAudioView audio control if a ShareAudioView was
  // provided during construction.  This method must not be called if the class
  // was created with share_audio_view == nullptr.
  void SetAudioSharingApprovedByUser(bool is_on);

 private:
  raw_ptr<ShareAudioView> share_audio_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PANE_VIEW_H_
