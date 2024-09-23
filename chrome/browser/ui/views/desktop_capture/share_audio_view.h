// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_AUDIO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_AUDIO_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"

class ShareAudioView : public views::View {
  METADATA_HEADER(ShareAudioView, views::View)

 public:
  ShareAudioView(const std::u16string& label_text, bool audio_offered);
  ShareAudioView(const ShareAudioView&) = delete;
  ShareAudioView& operator=(const ShareAudioView&) = delete;
  ~ShareAudioView() override;

  bool AudioOffered() const;
  bool IsAudioSharingApprovedByUser() const;
  // This method must only be called if the class was created with
  // audio_offered == true.
  void SetAudioSharingApprovedByUser(bool is_on);

  // Returns the text in the audio label if an audio label exists;
  // returns the empty string otherwise.
  std::u16string GetAudioLabelText() const;

 private:
  raw_ptr<views::Label> audio_toggle_label_ = nullptr;
  raw_ptr<views::ToggleButton> audio_toggle_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_AUDIO_VIEW_H_
