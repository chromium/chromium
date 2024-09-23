// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_COORDINATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/browser/ui/views/media_preview/mic_preview/audio_stream_coordinator.h"
#include "chrome/browser/ui/views/media_preview/mic_preview/mic_mediator.h"
#include "chrome/browser/ui/views/media_preview/mic_preview/mic_view_controller.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/view_tracker.h"

namespace media {
class AudioParameters;
struct AudioDeviceDescription;
}  // namespace media

// Acts as a middle man between the ViewController and the Mediator.
// Maintains the lifetime of its views.
class MicCoordinator {
 public:
  MicCoordinator(views::View& parent_view,
                 bool needs_borders,
                 const std::vector<std::string>& eligible_mic_ids,
                 PrefService& prefs,
                 bool allow_device_selection,
                 const media_preview_metrics::Context& metrics_context);
  MicCoordinator(const MicCoordinator&) = delete;
  MicCoordinator& operator=(const MicCoordinator&) = delete;
  ~MicCoordinator();

  // Invoked from the ViewController when a combobox selection has been made.
  void OnAudioSourceChanged(std::optional<size_t> selected_index);

  void UpdateDevicePreferenceRanking();

  const ui::SimpleComboboxModel& GetComboboxModelForTest() const {
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
  ui::SimpleComboboxModel combobox_model_;
  std::string active_device_id_;
  base::flat_set<std::string> eligible_mic_ids_;
  // This list must be kept in sync with the `combobox_model_` so that indices
  // align.
  std::vector<media::AudioDeviceDescription> eligible_device_infos_;
  raw_ptr<PrefService> prefs_;
  const bool allow_device_selection_;
  const media_preview_metrics::Context metrics_context_;
  std::optional<MicViewController> mic_view_controller_;
  std::optional<AudioStreamCoordinator> audio_stream_coordinator_;

  base::WeakPtrFactory<MicCoordinator> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_COORDINATOR_H_
