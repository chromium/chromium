// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_COORDINATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ui/views/media_preview/mic_preview/mic_mediator.h"
#include "chrome/browser/ui/views/media_preview/mic_preview/mic_selector_combobox_model.h"
#include "chrome/browser/ui/views/media_preview/mic_preview/mic_view_controller.h"
#include "ui/views/view_tracker.h"

namespace media {
class AudioParameters;
struct AudioDeviceDescription;
}  // namespace media

// Acts as a middle man between the ViewController and the Mediator.
// Maintains the lifetime of its views.
class MicCoordinator {
 public:
  MicCoordinator(views::View& parent_view, bool needs_borders);
  MicCoordinator(const MicCoordinator&) = delete;
  MicCoordinator& operator=(const MicCoordinator&) = delete;
  ~MicCoordinator();

  // Invoked from the ViewController when a combobox selection has been made.
  void OnAudioSourceChanged(std::optional<size_t> selected_index);

  const MicSelectorComboboxModel& GetComboboxModelForTest() const {
    return combobox_model_;
  }

 private:
  // `device_infos` is  a list of connected devices. When a new device
  // gets connected or a device gets disconnected, this function is called to
  // with the new list.
  void OnAudioSourceInfosReceived(
      const std::vector<media::AudioDeviceDescription>& device_infos);

  void ConnectAudioStream(
      const std::string& device_id,
      const std::optional<media::AudioParameters>& device_params);

  void ResetViewController();

  MicMediator mic_mediator_;
  views::ViewTracker mic_view_tracker_;
  MicSelectorComboboxModel combobox_model_;
  std::string active_device_id_;
  std::optional<MicViewController> mic_view_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_COORDINATOR_H_
