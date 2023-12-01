// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/mic_preview/mic_selector_combobox_model.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model_observer.h"

AudioSourceInfo::AudioSourceInfo(
    const media::AudioDeviceDescription& device_info,
    bool is_default)
    : id(device_info.unique_id),
      name(base::UTF8ToUTF16(device_info.device_name)),
      is_default(is_default) {}

AudioSourceInfo::~AudioSourceInfo() = default;
AudioSourceInfo::AudioSourceInfo(const AudioSourceInfo& other) = default;
AudioSourceInfo::AudioSourceInfo(AudioSourceInfo&& other) = default;

bool AudioSourceInfo::operator==(const AudioSourceInfo& other) const {
  return id == other.id && name == other.name && is_default == other.is_default;
}

MicSelectorComboboxModel::MicSelectorComboboxModel() = default;

MicSelectorComboboxModel::~MicSelectorComboboxModel() = default;

void MicSelectorComboboxModel::UpdateDeviceList(
    std::vector<AudioSourceInfo> audio_source_infos) {
  audio_source_infos_ = std::move(audio_source_infos);

  for (auto& observer : observers()) {
    observer.OnComboboxModelChanged(this);
  }
}

size_t MicSelectorComboboxModel::GetItemCount() const {
  // Item count will always be >= 1. In case of empty `audio_source_infos_`, a
  // message is shown to the user to connect a mic.
  return std::max(audio_source_infos_.size(), size_t(1));
}

std::u16string MicSelectorComboboxModel::GetItemAt(size_t index) const {
  if (audio_source_infos_.empty()) {
    CHECK_EQ(index, 0U);
    return l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_MICS_FOUND_COMBOBOX);
  }

  CHECK_LT(index, audio_source_infos_.size());
  return audio_source_infos_[index].name;
}

std::u16string MicSelectorComboboxModel::GetDropDownSecondaryTextAt(
    size_t index) const {
  if (audio_source_infos_.empty()) {
    return std::u16string();
  }

  CHECK_LT(index, audio_source_infos_.size());
  return audio_source_infos_[index].is_default
             ? l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_SYSTEM_DEFAULT_MIC)
             : std::u16string();
}

const AudioSourceInfo& MicSelectorComboboxModel::GetDeviceInfoAt(
    size_t index) const {
  CHECK_LT(index, audio_source_infos_.size());
  return audio_source_infos_[index];
}
