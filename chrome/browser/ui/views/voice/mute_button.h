// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_VOICE_MUTE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_VOICE_MUTE_BUTTON_H_

#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/md_text_button.h"

namespace voice {

// A button for muting the user's microphone. It runs its callback when
// pressed, and shows the state passed to SetMuted(). It shows a microphone
// while unmuted, a crossed-out one while muted.
class MuteButton : public views::MdTextButton {
  METADATA_HEADER(MuteButton, views::MdTextButton)

 public:
  MuteButton(PressedCallback callback,
             std::u16string mute_label,
             std::u16string unmute_label);
  MuteButton(const MuteButton&) = delete;
  MuteButton& operator=(const MuteButton&) = delete;
  ~MuteButton() override;

  void SetMuted(bool muted);

 private:
  void Update();

  const std::u16string mute_label_;
  const std::u16string unmute_label_;
  bool muted_ = false;
};

}  // namespace voice

#endif  // CHROME_BROWSER_UI_VIEWS_VOICE_MUTE_BUTTON_H_
