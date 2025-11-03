// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_PERMISSION_WARNING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_PERMISSION_WARNING_VIEW_H_

#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class AudioPermissionWarningView : public views::View {
  METADATA_HEADER(AudioPermissionWarningView, views::View)

 public:
  explicit AudioPermissionWarningView(
      base::RepeatingCallback<void()> cancel_callback);
  AudioPermissionWarningView(const AudioPermissionWarningView&) = delete;
  AudioPermissionWarningView& operator=(const AudioPermissionWarningView&) =
      delete;
  ~AudioPermissionWarningView() override;

  // views::View.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

 private:
  void OpenSystemSettings();

  base::RepeatingCallback<void()> cancel_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_PERMISSION_WARNING_VIEW_H_
