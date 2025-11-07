// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_PERMISSION_WARNING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_PERMISSION_WARNING_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
class MdTextButton;
}  // namespace views

class AudioPermissionWarningView : public views::View {
  METADATA_HEADER(AudioPermissionWarningView, views::View)

 public:
  explicit AudioPermissionWarningView(
      base::RepeatingCallback<void()> cancel_callback);
  AudioPermissionWarningView(const AudioPermissionWarningView&) = delete;
  AudioPermissionWarningView& operator=(const AudioPermissionWarningView&) =
      delete;
  ~AudioPermissionWarningView() override;

 private:
  void OpenSystemSettings();

  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  const base::RepeatingCallback<void()> cancel_callback_;

  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::MdTextButton> cancel_button_ = nullptr;
  raw_ptr<views::MdTextButton> system_settings_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_PERMISSION_WARNING_VIEW_H_
