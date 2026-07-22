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

namespace views {
class ToggleButton;
}

class DesktopMediaContentPaneView : public views::View {
  METADATA_HEADER(DesktopMediaContentPaneView, views::View)
 public:
  // Creates a pane-view with the supplied content_view. If a non-null
  // share_audio_view is provided, it is added below content_view.
  // If `show_audio_recommendation` is true (and `share_audio_view` is
  // non-null), an audio recommendation card is added between the separator and
  // the `share_audio_view`. `style_audio_toggle` specifies how the audio
  // sharing toggle should be styled.
  // TODO(crbug.com/340098903): Create ShareAudioView in the constructor.
  DesktopMediaContentPaneView(std::unique_ptr<views::View> content_view,
                              std::unique_ptr<ShareAudioView> share_audio_view,
                              bool show_audio_recommendation,
                              AudioSharingToggleStyle style_audio_toggle);

  DesktopMediaContentPaneView(const DesktopMediaContentPaneView&) = delete;
  DesktopMediaContentPaneView& operator=(const DesktopMediaContentPaneView&) =
      delete;
  ~DesktopMediaContentPaneView() override;

  bool AudioOffered() const;
  bool IsAudioSharingApprovedByUser() const;
  // Sets the state of the ShareAudioView audio control. This method must only
  // be called if the class was created with a non-null share_audio_view.
  void SetAudioSharingApprovedByUser(bool is_on);

  // Returns the text in the audio label if an audio label exists;
  // returns the empty string otherwise.
  std::u16string_view GetAudioLabelText() const;
  bool IsAudioRecommendationVisible() const;
  void SetAudioRecommendationVisible(bool visible);

  // Returns the audio sharing toggle button if it exists.
  views::ToggleButton* GetAudioToggleButtonForTesting() const;

#if BUILDFLAG(IS_MAC)
  void SetAudioWarningVisible(bool visible);
  bool IsAudioWarningVisible() const;
  void CancelAudioSharing();
#endif  // BUILDFLAG(IS_MAC)

 private:
  raw_ptr<views::View> audio_recommendation_view_ = nullptr;
  raw_ptr<ShareAudioView> share_audio_view_ = nullptr;
#if BUILDFLAG(IS_MAC)
  raw_ptr<AudioPermissionWarningView> audio_warning_view_ = nullptr;
#endif  // BUILDFLAG(IS_MAC)
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_CONTENT_PANE_VIEW_H_
