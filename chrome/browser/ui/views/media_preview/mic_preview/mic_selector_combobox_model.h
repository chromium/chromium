// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_SELECTOR_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_SELECTOR_COMBOBOX_MODEL_H_

#include <string>
#include <vector>

#include "ui/base/models/combobox_model.h"

namespace media {
struct AudioDeviceDescription;
}  // namespace media

// A struct to only store relevant info needed about each audio input device.
struct AudioSourceInfo {
  AudioSourceInfo(const media::AudioDeviceDescription& device_info,
                  bool is_default);
  AudioSourceInfo(const AudioSourceInfo& other);
  AudioSourceInfo& operator=(const AudioSourceInfo& other) = delete;
  AudioSourceInfo(AudioSourceInfo&& other);
  AudioSourceInfo& operator=(AudioSourceInfo&& other) = delete;
  ~AudioSourceInfo();

  bool operator==(const AudioSourceInfo& other) const;

  const std::string id;
  const std::u16string name;
  const bool is_default;
};

class MicSelectorComboboxModel : public ui::ComboboxModel {
 public:
  MicSelectorComboboxModel();
  MicSelectorComboboxModel(const MicSelectorComboboxModel&) = delete;
  MicSelectorComboboxModel& operator=(const MicSelectorComboboxModel&) = delete;
  ~MicSelectorComboboxModel() override;

  const AudioSourceInfo& GetDeviceInfoAt(size_t index) const;

  void UpdateDeviceList(std::vector<AudioSourceInfo> audio_source_infos);

  // ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::u16string GetDropDownSecondaryTextAt(size_t index) const override;

 private:
  std::vector<AudioSourceInfo> audio_source_infos_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_SELECTOR_COMBOBOX_MODEL_H_
