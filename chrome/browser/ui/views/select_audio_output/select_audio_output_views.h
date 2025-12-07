// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SELECT_AUDIO_OUTPUT_SELECT_AUDIO_OUTPUT_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SELECT_AUDIO_OUTPUT_SELECT_AUDIO_OUTPUT_VIEWS_H_

#include "base/functional/callback.h"
#include "chrome/browser/media/webrtc/select_audio_output_picker.h"
#include "content/public/browser/select_audio_output_request.h"
#include "ui/views/widget/widget.h"

class Browser;

// TODO(crbug.com/372214870): Merge this class with SelectAudioOutputDialog.
class SelectAudioOutputPickerViews : public SelectAudioOutputPicker {
 public:
  SelectAudioOutputPickerViews() = default;
  SelectAudioOutputPickerViews(const SelectAudioOutputPickerViews&) = delete;
  SelectAudioOutputPickerViews& operator=(const SelectAudioOutputPickerViews&) =
      delete;

  // Shows the audio output picker dialog.
  void Show(Browser* browser,
            const content::SelectAudioOutputRequest& request,
            content::SelectAudioOutputCallback callback) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SELECT_AUDIO_OUTPUT_SELECT_AUDIO_OUTPUT_VIEWS_H_
