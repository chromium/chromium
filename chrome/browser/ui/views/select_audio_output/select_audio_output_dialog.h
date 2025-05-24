// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SELECT_AUDIO_OUTPUT_SELECT_AUDIO_OUTPUT_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SELECT_AUDIO_OUTPUT_SELECT_AUDIO_OUTPUT_DIALOG_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "content/public/browser/select_audio_output_request.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/dialog_delegate.h"

class SelectAudioOutputDialog : public views::DialogDelegateView {
 public:
  SelectAudioOutputDialog(
      const std::vector<content::AudioOutputDeviceInfo>& audio_output_devices,
      content::SelectAudioOutputCallback callback);
  SelectAudioOutputDialog(const SelectAudioOutputDialog&) = delete;
  SelectAudioOutputDialog& operator=(const SelectAudioOutputDialog&) = delete;
  ~SelectAudioOutputDialog() override;

 private:
  void OnCancel();
  void OnAccept();
  void OnDeviceSelected(
      base::expected<std::string, content::SelectAudioOutputError> result);

  std::vector<content::AudioOutputDeviceInfo> audio_output_devices_;
  std::unique_ptr<ui::SimpleComboboxModel> combobox_model_;
  content::SelectAudioOutputCallback callback_;
  raw_ptr<views::Combobox> combobox_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SELECT_AUDIO_OUTPUT_SELECT_AUDIO_OUTPUT_DIALOG_H_
