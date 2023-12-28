// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_AUDIO_STREAM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_AUDIO_STREAM_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// The mic live audio feed view.
class AudioStreamView : public views::View {
  METADATA_HEADER(AudioStreamView, views::View)

 public:
  AudioStreamView();
  AudioStreamView(const AudioStreamView&) = delete;
  AudioStreamView& operator=(const AudioStreamView&) = delete;
  ~AudioStreamView() override;

  void ScheduleAudioStreamPaint(float audio_value);

  void Clear();

 protected:
  // views::View overrides
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  float last_audio_level_ = 0;
  const int rounded_radius_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_AUDIO_STREAM_VIEW_H_
