// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_CONTENT_PANE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_CONTENT_PANE_VIEW_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/desktop_capture/share_audio_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/desktop_capture/audio_permission_warning_view.h"
#endif  // BUILDFLAG(IS_MAC)

class DesktopMediaContentPaneView : public views::View {
  METADATA_HEADER(DesktopMediaContentPaneView, views::View)
 public:
  // Creates a pane-view with the supplied content_view. If a non-null
  // share_audio_view is provided, it is added below content_view.
  // TODO(crbug.com/340098903): Create ShareAudioView in the constructor.
  DesktopMediaContentPaneView(std::unique_ptr<views::View> content_view,
                              std::unique_ptr<ShareAudioView> share_audio_view);

  DesktopMediaContentPaneView(const DesktopMediaContentPaneView&) = delete;
  DesktopMediaContentPaneView& operator=(const DesktopMediaContentPaneView&) =
      delete;
  ~DesktopMediaContentPaneView() override;

  bool AudioOffered() const;
  bool IsAudioSharingApprovedByUser() const;
  // Sets the state of the ShareAudioView audio control. This method must only
  // be called if the class was created with a non-null share_audio_view.
  void SetAudioSharingApprovedByUser(bool is_on);

  bool IsAudioSharingControlEnabled() const;
  // Enables or disables the ShareAudioView audio control, meaning that while
  // the control is disabled the user cannot interact with it. This method must
  // only be called if the class was created with a non-null share_audio_view.
  void SetAudioSharingControlEnabled(bool enabled);
  // Returns the text in the audio label if an audio label exists;
  // returns the empty string otherwise.
  std::u16string_view GetAudioLabelText() const;
#if BUILDFLAG(IS_MAC)
  void SetAudioWarningVisible(bool visible);
  bool IsAudioWarningVisible() const;
  void CancelAudioSharing();
#endif  // BUILDFLAG(IS_MAC)

 private:
  raw_ptr<ShareAudioView> share_audio_view_ = nullptr;
#if BUILDFLAG(IS_MAC)
  raw_ptr<AudioPermissionWarningView> audio_warning_view_ = nullptr;
#endif  // BUILDFLAG(IS_MAC)
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_CONTENT_PANE_VIEW_H_
