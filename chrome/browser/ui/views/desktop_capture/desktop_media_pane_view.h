// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PANE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PANE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_content_pane_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout.h"

class DesktopMediaPermissionPaneViewMac;

class DesktopMediaPaneView : public views::View {
  METADATA_HEADER(DesktopMediaPaneView, views::View)
 public:
  // Creates a pane-view with the supplied content_view. If share_audio_view !=
  // nullptr, it is added below content_view.
  DesktopMediaPaneView(DesktopMediaList::Type type,
                       std::unique_ptr<views::View> content_view,
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

  // Returns the text in the audio label if an audio label exists;
  // returns the empty string otherwise.
  std::u16string GetAudioLabelText() const;

  bool IsPermissionPaneVisible() const;
  bool IsContentPaneVisible() const;

#if BUILDFLAG(IS_MAC)
  void OnScreenCapturePermissionUpdate(bool has_permission);
  bool WasPermissionButtonClicked() const;
#endif

 private:
#if BUILDFLAG(IS_MAC)
  bool PermissionRequired() const;
  void MakePermissionPaneView();
#endif

  const DesktopMediaList::Type type_;
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<DesktopMediaContentPaneView> content_pane_view_ = nullptr;
  raw_ptr<DesktopMediaPermissionPaneViewMac> permission_pane_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PANE_VIEW_H_
